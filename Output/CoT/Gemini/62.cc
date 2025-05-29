#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

int main(int argc, char *argv[]) {
  bool enablePacing = true;
  double simulationTime = 10.0;
  std::string dataRateBottleneck = "10Mbps";
  std::string delayBottleneck = "2ms";
  std::string dataRateOtherLinks = "100Mbps";
  std::string delayOtherLinks = "1ms";
  uint32_t packetSize = 1460;
  std::string tcpVariant = "TcpNewReno";
  uint32_t initialCwnd = 10;

  CommandLine cmd;
  cmd.AddValue("enablePacing", "Enable TCP pacing", enablePacing);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("dataRateBottleneck", "Data rate of the bottleneck link", dataRateBottleneck);
  cmd.AddValue("delayBottleneck", "Delay of the bottleneck link", delayBottleneck);
  cmd.AddValue("dataRateOtherLinks", "Data rate of other links", dataRateOtherLinks);
  cmd.AddValue("delayOtherLinks", "Delay of other links", delayOtherLinks);
  cmd.AddValue("packetSize", "TCP packet size", packetSize);
  cmd.AddValue("tcpVariant", "TCP variant to use", tcpVariant);
  cmd.AddValue("initialCwnd", "Initial congestion window", initialCwnd);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
  TypeId tid = TypeId::LookupByName("ns3::" + tcpVariant);
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(tid));
  Config::SetDefault("ns3::TcpSocketBase::InitialCwnd", UintegerValue(initialCwnd));

  NodeContainer nodes;
  nodes.Create(6);

  PointToPointHelper p2pBottleneck;
  p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(dataRateBottleneck));
  p2pBottleneck.SetChannelAttribute("Delay", StringValue(delayBottleneck));

  PointToPointHelper p2pOther;
  p2pOther.SetDeviceAttribute("DataRate", StringValue(dataRateOtherLinks));
  p2pOther.SetChannelAttribute("Delay", StringValue(delayOtherLinks));

  NetDeviceContainer devices1 = p2pOther.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = p2pOther.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devices3 = p2pBottleneck.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer devices4 = p2pOther.Install(nodes.Get(3), nodes.Get(4));
  NetDeviceContainer devices5 = p2pOther.Install(nodes.Get(4), nodes.Get(5));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);
  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);
  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces5 = address.Assign(devices5);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 50000;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1.0));

  UdpEchoClientHelper echoClient(interfaces5.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 1.0));

  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(5));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime));

  BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces3.GetAddress(1), port));
  bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(2));
  sourceApps.Start(Seconds(2.0));
  sourceApps.Stop(Seconds(simulationTime - 1.0));

  Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(2), TcpSocketFactory::GetTypeId());
  socket->Bind();
  socket->Connect(InetSocketAddress(interfaces3.GetAddress(1), port));
  if(enablePacing){
        socket->SetAttribute("TcpNoDelay", BooleanValue(true));
        socket->SetAttribute("UsePacing", BooleanValue(true));
  }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(simulationTime));

  AsciiTraceHelper ascii;
  p2pBottleneck.EnableAsciiAll(ascii.CreateFileStream("tcp-pacing.tr"));

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
  }

  monitor->SerializeToXmlFile("tcp-pacing.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}