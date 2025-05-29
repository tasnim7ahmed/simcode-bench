#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create wired nodes
  NodeContainer wiredNodes;
  wiredNodes.Create(4);

  // Create wireless nodes
  NodeContainer wirelessNodes;
  wirelessNodes.Create(3);

  // Configure point-to-point links for wired nodes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer wiredDevices;
  for (uint32_t i = 0; i < 3; ++i) {
    NetDeviceContainer linkDevices = pointToPoint.Install(NodeContainer(wiredNodes.Get(i), wiredNodes.Get(i + 1)));
    wiredDevices.Add(linkDevices.Get(0));
    wiredDevices.Add(linkDevices.Get(1));
  }

  // Configure Wi-Fi for wireless nodes
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, wirelessNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wiredNodes.Get(0)); // First wired node as AP

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(wiredNodes);
  stack.Install(wirelessNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wiredInterfaces = address.Assign(wiredDevices);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

  // Mobility for wireless nodes
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0.0"),
                                "Y", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wirelessNodes);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


  // Create TCP client application on wireless nodes and server on wired nodes
  uint16_t port = 50000;
  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(wiredInterfaces.GetAddress(5), port)); // Send to last wired node
  source.SetAttribute("MaxBytes", UintegerValue(0));

  ApplicationContainer sourceApps;
  for(uint32_t i=0; i<3; ++i){
      sourceApps.Add(source.Install(wirelessNodes.Get(i)));
      sourceApps.Get(i)->SetStartTime(Seconds(2.0));
      sourceApps.Get(i)->SetStopTime(Seconds(10.0));
  }
  

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(wiredNodes.Get(3));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("hybrid_network");
  phy.EnablePcapAll("hybrid_network");

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Calculate throughput
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  double total_throughput = 0.0;
  int flow_count = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationPort == port) {
        total_throughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1000000;
        flow_count++;
    }
  }
  
  std::cout << "Average throughput: " << total_throughput/flow_count << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}