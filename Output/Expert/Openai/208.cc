#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/wifi-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHandoverSimulation");

uint32_t g_handoverCount = 0;
uint32_t g_packetLostCount = 0;

void HandoverTracer(Mac48Address oldAp, Mac48Address newAp)
{
  ++g_handoverCount;
}

void RxDrop(std::string context, Ptr<const Packet> p)
{
  ++g_packetLostCount;
}

int main(int argc, char *argv[])
{
  uint32_t nStations = 6;
  double simTime = 30.0;
  double distance = 40.0;
  double apSeparation = 60.0;
  std::string phyMode = "HtMcs7";
  double txPower = 16.0;

  CommandLine cmd;
  cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
  cmd.Parse(argc, argv);

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(nStations);

  // Wi-Fi helpers
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "ReferenceDistance", DoubleValue(1.0), "Exponent", DoubleValue(3.0));
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(txPower));
  phy.Set("TxPowerEnd", DoubleValue(txPower));
  phy.Set("EnergyDetectionThreshold", DoubleValue(-89.0));
  phy.Set("CcaMode1Threshold", DoubleValue(-62.0));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ap-ssid-1");
  Ssid ssid2 = Ssid("ap-ssid-2");

  NetDeviceContainer apDevices, staDevices[2];

  // AP 1
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
  apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));
  // AP 2
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
  apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

  // Stations: Each connects to both APs (simulate scanning)
  mac.SetType("ns3::StaWifiMac", "ActiveProbing", BooleanValue(true));
  // STA to AP1 (SSID1)
  staDevices[0] = wifi.Install(phy, mac, staNodes);
  // STA to AP2 (SSID2)
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(true));
  staDevices[1] = wifi.Install(phy, mac, staNodes);

  // Configure mobility
  MobilityHelper mobilityAp, mobilitySta;
  Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
  apPos->Add(Vector(distance, distance, 0.0));
  apPos->Add(Vector(apSeparation + distance, distance, 0.0));
  mobilityAp.SetPositionAllocator(apPos);
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(apNodes);
  mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=40.0]"));
  mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue("Time"),
                               "Time", TimeValue(Seconds(0.5)),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                               "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 40.0)));
  mobilitySta.Install(staNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer apInterfaces, staInterfaces;

  address.SetBase("10.1.1.0", "255.255.255.0");
  apInterfaces = address.Assign(apDevices);
  address.Assign(staDevices[0]);
  address.Assign(staDevices[1]);

  // Install UDP applications
  uint16_t port = 9;

  // Packet sink on each AP
  ApplicationContainer sinkApps;
  for (uint32_t j=0; j<2; ++j)
  {
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(j), port));
    sinkApps.Add(sinkHelper.Install(apNodes.Get(j)));
  }

  // Each station sends to AP1 and AP2 alternately (simulate roaming/assoc changes)
  ApplicationContainer sourceApps;
  for (uint32_t i = 0; i < nStations; ++i)
  {
    UdpClientHelper client1(apInterfaces.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(10000));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client1.SetAttribute("PacketSize", UintegerValue(512));
    sourceApps.Add(client1.Install(staNodes.Get(i)));

    UdpClientHelper client2(apInterfaces.GetAddress(1), port);
    client2.SetAttribute("MaxPackets", UintegerValue(10000));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client2.SetAttribute("PacketSize", UintegerValue(512));
    sourceApps.Add(client2.Install(staNodes.Get(i)));
  }

  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simTime));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(simTime));

  // Enable handover/handoff detection (track association changes)
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback(&HandoverTracer));
  // Count packet drops at Wi-Fi MAC layer
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/PhyRxDrop", MakeCallback(&RxDrop));

  // Enable NetAnim
  AnimationInterface anim("wifi-handover.xml");
  for (uint32_t i = 0; i < 2; ++i)
  {
    anim.UpdateNodeDescription(apNodes.Get(i), "AP");
    anim.UpdateNodeColor(apNodes.Get(i), 255, 0, 0);
  }
  for (uint32_t i = 0; i < nStations; ++i)
  {
    anim.UpdateNodeDescription(staNodes.Get(i), "STA");
    anim.UpdateNodeColor(staNodes.Get(i), 0, 0, 255);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  std::cout << "Total handover (re-association) events: " << g_handoverCount << std::endl;
  std::cout << "Total packet loss (at MAC): " << g_packetLostCount << std::endl;

  Simulator::Destroy();
  return 0;
}