#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHandoverSimulation");

uint32_t handoverCount = 0;
uint32_t packetLossCount = 0;
std::map<uint32_t, Mac48Address> lastApAssociation;

void
NotifyAssoc (std::string context, Mac48Address apAddress, Mac48Address staAddress)
{
  uint32_t nodeId = atoi (context.substr(context.find_last_of("/") + 1).c_str());
  if (lastApAssociation[nodeId] != Mac48Address() && lastApAssociation[nodeId] != apAddress)
    {
      handoverCount++;
    }
  lastApAssociation[nodeId] = apAddress;
}

void
RxDrop (Ptr<const Packet> p)
{
  packetLossCount++;
}

int
main (int argc, char *argv[])
{
  uint32_t numStas = 6;
  double simTime = 30.0;
  double areaX = 80.0, areaY = 50.0;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (numStas);

  NodeContainer wifiApNodes;
  wifiApNodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns3-ssid-ap1");
  Ssid ssid2 = Ssid ("ns3-ssid-ap2");

  // Each STA supports both SSIDs
  NetDeviceContainer staDevices1, staDevices2, apDevice1, apDevice2;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (true));
  staDevices1 = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (true));
  staDevices2 = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1));
  apDevice1 = wifi.Install (phy, mac, wifiApNodes.Get(0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2));
  apDevice2 = wifi.Install (phy, mac, wifiApNodes.Get(1));

  MobilityHelper mobility;

  // AP positions
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (20.0, areaY / 2, 0.0)); // AP1
  positionAlloc->Add (Vector (60.0, areaY / 2, 0.0)); // AP2
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (wifiApNodes);

  // Random walk for STAs
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=80.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, areaX, 0, areaY)),
                             "Distance", DoubleValue (10.0),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"));
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIf1 = address.Assign (staDevices1);
  address.NewNetwork ();
  Ipv4InterfaceContainer apIf1 = address.Assign (apDevice1);
  address.NewNetwork ();
  Ipv4InterfaceContainer staIf2 = address.Assign (staDevices2);
  address.NewNetwork ();
  Ipv4InterfaceContainer apIf2 = address.Assign (apDevice2);

  // Applications: send UDP packets from each STA to AP1
  uint16_t port = 4000;
  for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
      UdpClientHelper client (apIf1.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (10000));
      client.SetAttribute ("Interval", TimeValue (MilliSeconds (200)));
      client.SetAttribute ("PacketSize", UintegerValue (200));
      ApplicationContainer clientApp = client.Install (wifiStaNodes.Get (i));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (simTime));

    }

  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiApNodes.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // Trace AP association - For both SSID macs of each STA node
  for (uint32_t i=0; i< numStas; ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << wifiStaNodes.Get(i)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Association";
      Config::Connect (oss.str(), MakeBoundCallback (&NotifyAssoc, wifiApNodes.Get(0)->GetObject<NetDevice>()->GetAddress ()));
      oss.str("");
      oss << "/NodeList/" << wifiStaNodes.Get(i)->GetId() << "/DeviceList/1/$ns3::WifiNetDevice/Association";
      Config::Connect (oss.str(), MakeBoundCallback (&NotifyAssoc, wifiApNodes.Get(1)->GetObject<NetDevice>()->GetAddress ()));
    }

  // Trace packet drops at MAC/PHY
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/PhyRxDrop", MakeCallback (&RxDrop));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRxDrop", MakeCallback (&RxDrop));

  // NetAnim
  AnimationInterface anim ("wifi-handover.xml");
  anim.SetMobilityPollInterval (Seconds (0.5));
  for (uint32_t i = 0; i < wifiApNodes.GetN (); ++i)
    {
      anim.UpdateNodeDescription (wifiApNodes.Get (i), "AP");
      anim.UpdateNodeColor (wifiApNodes.Get (i), 255, 0, 0);
    }
  for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
      anim.UpdateNodeDescription (wifiStaNodes.Get (i), "STA");
      anim.UpdateNodeColor (wifiStaNodes.Get (i), 0, 0, 255);
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  std::cout << "Simulation completed." << std::endl;
  std::cout << "Total handover events: " << handoverCount << std::endl;
  std::cout << "Total packet drops: " << packetLossCount << std::endl;

  Simulator::Destroy ();
  return 0;
}