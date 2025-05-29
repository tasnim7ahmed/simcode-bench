#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enableRtsCts = false;

  CommandLine cmd;
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS (0=false, 1=true)", enableRtsCts);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Configure RTS/CTS
  if (enableRtsCts) {
    Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", StringValue("100"));
  } else {
    Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", StringValue("2200")); // Disabled
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(50.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  OnOffHelper client0("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
  client0.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client0.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client0.SetAttribute("PacketSize", UintegerValue(1024));
  client0.SetAttribute("DataRate", StringValue("1Mb/s"));
  ApplicationContainer clientApps0 = client0.Install(nodes.Get(0));
  clientApps0.Start(Seconds(2.0));
  clientApps0.Stop(Seconds(10.0));

  OnOffHelper client2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
  client2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client2.SetAttribute("PacketSize", UintegerValue(1024));
  client2.SetAttribute("DataRate", StringValue("1Mb/s"));
  ApplicationContainer clientApps2 = client2.Install(nodes.Get(2));
  clientApps2.Start(Seconds(3.0));
  clientApps2.Stop(Seconds(10.0));

  // Propagation Loss Model
  Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
  lossModel->SetDefaultLoss(50.0);
  phy.AddPropagationLossModel(lossModel);
  lossModel->SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 200.0); //Hidden Terminal

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto it = stats.begin(); it != stats.end(); ++it) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << it->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << it->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << it->second.lostPackets << "\n";
    std::cout << "  Throughput: " << it->second.txBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}