#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0;
  double rss = -50;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if true", verbose);
  cmd.AddValue("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("rss", "Fixed RSS value (dBm)", rss);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Set up the RSS loss model
  Ptr<Node> apNode = nodes.Get(1);
  Ptr<MobilityModel> apMobility = apNode->GetObject<MobilityModel>();
  Ptr<ConstantPositionMobilityModel> apConstMob = DynamicCast<ConstantPositionMobilityModel>(apMobility);
  apConstMob->SetPosition(Vector(0.0, 0.0, 0.0));

  Ptr<Node> staNode = nodes.Get(0);
  Ptr<MobilityModel> staMobility = staNode->GetObject<MobilityModel>();
  Ptr<ConstantPositionMobilityModel> staConstMob = DynamicCast<ConstantPositionMobilityModel>(staMobility);
  staConstMob->SetPosition(Vector(1.0, 0.0, 0.0)); // Station at (1,0,0)

  Ptr<WifiNetDevice> apNetDevice = DynamicCast<WifiNetDevice>(apDevices.Get(0));
  Ptr<WifiNetDevice> staNetDevice = DynamicCast<WifiNetDevice>(staDevices.Get(0));

  Ptr<YansWifiPhy> apPhy = apNetDevice->GetPHY();
  Ptr<YansWifiPhy> staPhy = staNetDevice->GetPHY();

  Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel>();
  lossModel->SetRss(rss);
  apPhy->SetPathLossModel(lossModel);
  staPhy->SetPathLossModel(lossModel);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer apps = server.Install(nodes.Get(1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(numPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  apps = client.Install(nodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  phy.EnablePcapAll("wifi-fixedrss");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  AnimationInterface anim("wifi-fixedrss.xml");
  anim.SetConstantPosition(nodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(nodes.Get(1), 2.0, 2.0);

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();

  return 0;
}