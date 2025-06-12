#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t tcpPort = 5000;
  uint16_t udpPort = 5001;

  // TCP setup
  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
  bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024));
  ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
  sourceApps.Start(Seconds(1.0));
  sourceApps.Stop(Seconds(10.0));

  PacketSinkHelper packetSinkHelperTcp("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
  ApplicationContainer sinkAppsTcp = packetSinkHelperTcp.Install(nodes.Get(1));
  sinkAppsTcp.Start(Seconds(1.0));
  sinkAppsTcp.Stop(Seconds(10.0));


  // UDP setup
  UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  udpClient.SetAttribute("Interval", TimeValue(Time("0.0001")));
  udpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  PacketSinkHelper packetSinkHelperUdp("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
  ApplicationContainer sinkAppsUdp = packetSinkHelperUdp.Install(nodes.Get(1));
  sinkAppsUdp.Start(Seconds(1.0));
  sinkAppsUdp.Stop(Seconds(10.0));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("tcp-udp-comparison.xml");

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalTcpRxBytes = 0.0;
  double totalUdpRxBytes = 0.0;

  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
      if (t.destinationPort == tcpPort) {
          totalTcpRxBytes += iter->second.rxBytes;
      } else if (t.destinationPort == udpPort) {
          totalUdpRxBytes += iter->second.rxBytes;
      }
  }

  double tcpThroughput = (totalTcpRxBytes * 8.0) / (10.0 * 1000000.0);
  double udpThroughput = (totalUdpRxBytes * 8.0) / (10.0 * 1000000.0);


  std::cout << "TCP Throughput: " << tcpThroughput << " Mbps" << std::endl;
  std::cout << "UDP Throughput: " << udpThroughput << " Mbps" << std::endl;


  Simulator::Destroy();

  return 0;
}