#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(3);

  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d0d1 = pointToPoint.Install(n0n1);
  NetDeviceContainer d1d2 = pointToPoint.Install(n1n2);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(i1i2.GetAddress(1,0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<BulkSendApplication> sourceApp = CreateObject<BulkSendApplication>();
  sourceApp->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
  sourceApp->SetAttribute("SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));
  sourceApp->SetAttribute("Remote", AddressValue(InetSocketAddress(i1i2.GetAddress(1,0), port)));
  sourceApp->SetAttribute("SendSize", UintegerValue(1024));
  sourceApp->SetAttribute("MaxBytes", UintegerValue(0));
  nodes.Get(0)->AddApplication(sourceApp);
  sourceApp->SetStartTime(Seconds(2.0));
  sourceApp->SetStopTime(Seconds(10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
  }

  monitor->SerializeToXmlFile("tcp-example.xml", false, true);

  Simulator::Destroy();
  return 0;
}