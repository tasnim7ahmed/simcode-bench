#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiHandoverSimulation");

class HandoverStats {
public:
  uint32_t m_handovers = 0;
  uint64_t m_packetsLost = 0;

  void CountHandover() {
    m_handovers++;
  }

  void CountPacketsLost(uint32_t num) {
    m_packetsLost += num;
  }
};

void
NotifySwitch (Ptr<HandoverStats> stats, Mac48Address oldAp, Mac48Address newAp)
{
  std::cout << Simulator::Now ().GetSeconds () << "s: Node switched from AP " << oldAp << " to AP " << newAp << std::endl;
  stats->CountHandover();
}

void
PacketLossCallback (Ptr<HandoverStats> stats, uint32_t packets)
{
  stats->CountPacketsLost(packets);
}

int main(int argc, char *argv[]) {
  uint32_t numNodes = 6;
  double simTime = 10.0;
  bool enableNetAnim = true;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Number of mobile nodes", numNodes);
  cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
  cmd.AddValue("enableNetAnim", "Enable NetAnim output", enableNetAnim);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(numNodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::AarfcdWifiManager");

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("network-AP1");
  Ssid ssid2 = Ssid("network-AP2");

  // Install APs
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices1;
  apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices2;
  apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

  // Install STAs
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices1;
  staDevices1 = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices2;
  staDevices2 = wifi.Install(phy, mac, staNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(staNodes);

  // Fix AP positions
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(10.0, 50.0, 0.0)); // AP1
  positionAlloc->Add(Vector(90.0, 50.0, 0.0)); // AP2

  MobilityHelper apMobility;
  apMobility.SetPositionAllocator(positionAlloc);
  apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  apMobility.Install(apNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
  Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
  address.NewNetwork();
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices1);
  staInterfaces.Add(address.Assign(staDevices2));

  // UDP Echo server on both APs
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(apNodes);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simTime));

  // UDP Echo clients on all STAs
  UdpEchoClientHelper echoClient1(apInterfaces1.GetAddress(0), 9);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

  UdpEchoClientHelper echoClient2(apInterfaces2.GetAddress(0), 9);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps1 = echoClient1.Install(staNodes);
  ApplicationContainer clientApps2 = echoClient2.Install(staNodes);

  clientApps1.Start(Seconds(0.5));
  clientApps1.Stop(Seconds(simTime));
  clientApps2.Start(Seconds(0.5));
  clientApps2.Stop(Seconds(simTime));

  // Handover tracking
  Ptr<HandoverStats> stats = CreateObject<HandoverStats>();

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/AssocStateChange",
                  MakeBoundCallback(&NotifySwitch, stats));

  // Packet loss tracking
  for (uint32_t i = 0; i < clientApps1.GetN(); ++i) {
    clientApps1.Get(i)->TraceConnectWithoutContext("TxWithLoss", MakeBoundCallback(&PacketLossCallback, stats));
  }
  for (uint32_t i = 0; i < clientApps2.GetN(); ++i) {
    clientApps2.Get(i)->TraceConnectWithoutContext("TxWithLoss", MakeBoundCallback(&PacketLossCallback, stats));
  }

  // Animation
  if (enableNetAnim) {
    AnimationInterface anim("wifi-handover.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("wifi-route.tr", Seconds(0), Seconds(simTime), Seconds(0.25));
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  std::cout << "Total handovers: " << stats->m_handovers << std::endl;
  std::cout << "Total packets lost: " << stats->m_packetsLost << std::endl;

  Simulator::Destroy();
  return 0;
}