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
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
  LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

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

  // UDP Application
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // TCP Application
  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("simulation.xml");
  anim.SetConstantPosition(nodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(nodes.Get(1), 2.0, 2.0);

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress
              << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << std::endl;
    std::cout << "  Tx Bytes:   " << i->second.txBytes << std::endl;
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << std::endl;
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps" << std::endl;
    std::cout << "  Packet Loss Ratio: " << (double)i->second.lostPackets / (double)i->second.txPackets << std::endl;
    std::cout << "  Delay Sum: " << i->second.delaySum << std::endl;
  }

  monitor->SerializeToXmlFile("simulation.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}