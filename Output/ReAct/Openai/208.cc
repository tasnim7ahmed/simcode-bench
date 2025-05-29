#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoApHandoverSimulation");

// Global stats
uint32_t g_handoverCount = 0;
uint32_t g_packetLost = 0;

std::map<uint32_t, Mac48Address> g_staApMap;

// Callback for handover (association change)
void AssocCallback(std::string context, Mac48Address oldAp, Mac48Address newAp)
{
  if (oldAp != Mac48Address() && oldAp != newAp)
    {
      g_handoverCount++;
      std::cout << Simulator::Now().GetSeconds()
                << "s: " << context
                << " handover from [" << oldAp << "]"
                << " to [" << newAp << "]"
                << " count=" << g_handoverCount << std::endl;
    }
}

// Callback for packet loss
void RxDrop(PacketSink *sink, Ptr<const Packet> p, const Address &from)
{
  g_packetLost++;
}

int main(int argc, char *argv[])
{
  uint32_t nSta = 6;
  double simTime = 60.0;
  double areaX = 100.0;
  double areaY = 50.0;

  CommandLine cmd;
  cmd.AddValue("nSta", "Number of stations", nSta);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(nSta);

  // Wi-Fi configuration
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("wifi-ap1");
  Ssid ssid2 = Ssid("wifi-ap2");

  // Install AP devices
  NetDeviceContainer apDevices[2];

  for (uint32_t i = 0; i < 2; ++i)
    {
      mac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(i == 0 ? ssid1 : ssid2));
      apDevices[i] = wifi.Install(phy, mac, apNodes.Get(i));
    }

  // Install STA devices (each node will scan both SSIDs for roaming)
  NetDeviceContainer staDevices;
  mac.SetType("ns3::StaWifiMac",
              "ActiveProbing", BooleanValue(true));

  for (uint32_t i = 0; i < nSta; ++i)
    {
      NetDeviceContainer staDev;
      staDev = wifi.Install(phy, mac, staNodes.Get(i));
      staDevices.Add(staDev);
    }

  // Mobility: place APs statically
  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
  apPositionAlloc->Add(Vector(20.0, areaY/2, 0.0)); // AP1
  apPositionAlloc->Add(Vector(areaX-20, areaY/2, 0.0)); // AP2

  MobilityHelper mobilityAp;
  mobilityAp.SetPositionAllocator(apPositionAlloc);
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(apNodes);

  // STA mobility: RandomWalk2d within area
  MobilityHelper mobilitySta;
  mobilitySta.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue ("Uniform(0.0," + std::to_string(areaX) + ")"),
                                   "Y", StringValue ("Uniform(0.0," + std::to_string(areaY) + ")"));
  mobilitySta.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (0, areaX, 0, areaY)),
                                "Speed", StringValue("Uniform(1.0,3.0)"),
                                "Distance", DoubleValue(4.0));
  mobilitySta.Install (staNodes);

  // Internet + IP addressing
  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  // Each AP on a /24, routed
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apIfaces[2], staIfaces;
  apIfaces[0] = address.Assign(apDevices[0]);
  address.NewNetwork();
  apIfaces[1] = address.Assign(apDevices[1]);
  address.NewNetwork();
  staIfaces = address.Assign(staDevices);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Applications (UDP server/client)
  uint16_t port = 4000;
  ApplicationContainer servers;
  for (uint32_t i = 0; i < 2; ++i)
    {
      UdpServerHelper udpServer(port);
      servers.Add(udpServer.Install(apNodes.Get(i)));
    }
  servers.Start(Seconds(1.0));
  servers.Stop(Seconds(simTime));

  ApplicationContainer clients;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      // Each STA sends to both APs, so will always have at least one reachable
      UdpClientHelper udpClient1(apIfaces[0].GetAddress(0), port);
      udpClient1.SetAttribute("MaxPackets", UintegerValue(1000000));
      udpClient1.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
      udpClient1.SetAttribute("PacketSize", UintegerValue(200));

      UdpClientHelper udpClient2(apIfaces[1].GetAddress(0), port);
      udpClient2.SetAttribute("MaxPackets", UintegerValue(1000000));
      udpClient2.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
      udpClient2.SetAttribute("PacketSize", UintegerValue(200));

      clients.Add(udpClient1.Install(staNodes.Get(i)));
      clients.Add(udpClient2.Install(staNodes.Get(i)));
    }
  clients.Start(Seconds(2.0));
  clients.Stop(Seconds(simTime));

  // Association/Handover tracing
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&RxDrop));

  for (uint32_t i = 0; i < nSta; ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << staNodes.Get(i)->GetId()
          << "/DeviceList/0/$ns3::WifiNetDevice/Mac/Assoc";
      Config::Connect(oss.str(), MakeBoundCallback(&AssocCallback));
    }

  // Packet loss tracing
  Ptr<UdpServer> udpServerApp1 = DynamicCast<UdpServer>(servers.Get(0));
  Ptr<UdpServer> udpServerApp2 = DynamicCast<UdpServer>(servers.Get(1));
  Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(udpServerApp1);
  Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(udpServerApp2);

  if (sink1 && sink2)
    {
      sink1->TraceConnectWithoutContext("RxDrop", MakeCallback(&RxDrop));
      sink2->TraceConnectWithoutContext("RxDrop", MakeCallback(&RxDrop));
    }

  // NetAnim visualization
  AnimationInterface anim("handover_netanim.xml");
  for (uint32_t i=0; i<apNodes.GetN(); ++i)
    anim.UpdateNodeDescription(apNodes.Get(i), "AP"+std::to_string(i+1));
  for (uint32_t i=0; i<staNodes.GetN(); ++i)
    anim.UpdateNodeDescription(staNodes.Get(i), "STA"+std::to_string(i+1));
  anim.EnablePacketMetadata(true);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  std::cout << "Total handovers detected: " << g_handoverCount << std::endl;
  std::cout << "Total packet losses during handover: " << g_packetLost << std::endl;

  Simulator::Destroy();
  return 0;
}