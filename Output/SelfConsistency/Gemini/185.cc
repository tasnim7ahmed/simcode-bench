#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMobilityExample");

int main(int argc, char *argv[]) {
  bool enableNetAnim = true;

  CommandLine cmd;
  cmd.AddValue("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.Parse(argc, argv);

  // Disable fragmentation (optional)
  Config::SetDefault("ns3::WifiMacHeader::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiMacHeader::RtsCtsThreshold", StringValue("2300"));

  // Create Nodes: APs and STAs
  NodeContainer apNodes;
  apNodes.Create(3);

  NodeContainer staNodes;
  staNodes.Create(6);

  // Configure Wi-Fi PHY
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  // Configure Wi-Fi MAC for APs (infrastructure mode)
  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

  // Configure Wi-Fi MAC for STAs (station mode)
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));

  mobility.Install(staNodes);

  // Assign fixed positions to APs
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(10.0, 10.0, 0.0));
  positionAlloc->Add(Vector(30.0, 20.0, 0.0));
  positionAlloc->Add(Vector(50.0, 10.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // Create UDP traffic flows
  UdpClientServerHelper udp(9);
  udp.SetAttribute("MaxPackets", UintegerValue(1000));
  udp.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  udp.SetAttribute("PacketSize", UintegerValue(1024));

  // Define server address (arbitrarily using the first AP interface)
  Address serverAddress(InetSocketAddress(apInterfaces.GetAddress(0), 9));

  // Install UDP client on each STA, directed towards the server
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
    Ptr<Node> clientNode = staNodes.Get(i);
    Ptr<Application> client = udp.InstallClient(clientNode);
    Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(client); // Get the UdpClient pointer
    udpClient->SetAttribute("RemoteAddress", AddressValue(serverAddress));
    clientApps.Add(client);
  }

  // Install UDP server on the first AP
  ApplicationContainer serverApps = udp.InstallServer(apNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable NetAnim visualization
  if (enableNetAnim) {
    AnimationInterface anim("wifi-mobility.xml");
    anim.SetConstantPosition(apNodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(apNodes.Get(1), 30.0, 20.0);
    anim.SetConstantPosition(apNodes.Get(2), 50.0, 10.0);
  }

  // Enable FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}