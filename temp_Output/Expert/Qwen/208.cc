#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessHandoverSimulation");

class HandoverStats {
public:
  uint32_t handovers = 0;
  uint64_t packetsLost = 0;

  void IncrementHandovers() { ++handovers; }
  void AddPacketsLost(uint64_t lost) { packetsLost += lost; }
};

void
NotifyHandover(std::string context, Mac48Address oldAp, Mac48Address newAp, HandoverStats *stats)
{
  if (oldAp != newAp)
    {
      NS_LOG_INFO("Handover: " << oldAp << " -> " << newAp);
      stats->IncrementHandovers();
    }
}

void
OnRxDrop(Ptr<const Packet> packet, const WifiMacHeader *header, HandoverStats *stats)
{
  stats->AddPacketsLost(1);
}

int
main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("1500"));
  Config::SetDefault("ns3::WifiPhy::ChannelSettings", StringValue("{0, 0, 0, 0}"));
  Config::SetDefault("ns3::WifiHelper::ProtectionMode", StringValue("Dcf"));
  Config::SetDefault("ns3::WifiHelper::FrameExchangeManagerType", TypeIdValue(DcaTxop::GetTypeId()));

  NodeContainer wifiApNodes;
  wifiApNodes.Create(2);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(6);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("AP1-Network");
  Ssid ssid2 = Ssid("AP2-Network");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices1 = wifi.Install(phy, mac, wifiApNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(2.5)));
  NetDeviceContainer apDevices2 = wifi.Install(phy, mac, wifiApNodes.Get(1));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

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
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                            "Distance", DoubleValue(5.0));
  mobility.Install(wifiStaNodes);

  // Static positions for APs
  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
  apPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
  apPositionAlloc->Add(Vector(100.0, 0.0, 0.0));
  mobility.SetPositionAllocator(apPositionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNodes);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
  Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // Connect handover notifications
  HandoverStats stats;
  Config::Connect("/NodeList/*/DeviceList/*/Mac/AssocManager/HandoverStart",
                  MakeBoundCallback(&NotifyHandover, &stats));

  // Capture dropped packets
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/Phy/RxError",
                                MakeBoundCallback(&OnRxDrop, &stats));

  // Install applications
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(wifiApNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  UdpEchoClientHelper echoClient(apInterfaces1.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(wifiStaNodes);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  // Animation
  AnimationInterface anim("wireless_handover.xml");
  anim.SetMobilityPollInterval(Seconds(0.1));

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "Total Handovers: " << stats.handovers << std::endl;
  std::cout << "Total Packets Lost: " << stats.packetsLost << std::endl;

  return 0;
}