#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourNodeTopology");

void
PacketReceiveCallback(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream recvLog("packet-receptions.txt", std::ios::app);
  recvLog << Simulator::Now().GetSeconds() << "s\tReceived packet of size " << packet->GetSize() << " bytes\n";
}

int main(int argc, char *argv[])
{
  uint32_t n1n3Metric = 10;
  CommandLine cmd;
  cmd.AddValue("n1n3Metric", "Routing metric for link n1-n3", n1n3Metric);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);
  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n1 = nodes.Get(1);
  Ptr<Node> n2 = nodes.Get(2);
  Ptr<Node> n3 = nodes.Get(3);

  // Define node containers for links
  NodeContainer n0n2(n0, n2);
  NodeContainer n1n2(n1, n2);
  NodeContainer n1n3(n1, n3);
  NodeContainer n2n3(n2, n3);

  // Point-to-point helpers
  PointToPointHelper p2pn0n2, p2pn1n2, p2pn1n3, p2pn2n3;

  p2pn0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2pn0n2.SetChannelAttribute("Delay", StringValue("2ms"));

  p2pn1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2pn1n2.SetChannelAttribute("Delay", StringValue("2ms"));

  p2pn1n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2pn1n3.SetChannelAttribute("Delay", StringValue("100ms"));

  p2pn2n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2pn2n3.SetChannelAttribute("Delay", StringValue("10ms"));

  // Install devices
  NetDeviceContainer d0n2 = p2pn0n2.Install(n0n2);
  NetDeviceContainer d1n2 = p2pn1n2.Install(n1n2);
  NetDeviceContainer d1n3 = p2pn1n3.Install(n1n3);
  NetDeviceContainer d2n3 = p2pn2n3.Install(n2n3);

  // Install stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0n2 = ipv4.Assign(d0n2);

  ipv4.SetBase("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1n2 = ipv4.Assign(d1n2);

  ipv4.SetBase("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i1n3 = ipv4.Assign(d1n3);

  ipv4.SetBase("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i2n3 = ipv4.Assign(d2n3);

  // Set manual routing metric for n1-n3
  Ptr<Ipv4> ipv4n1 = n1->GetObject<Ipv4>();
  uint32_t ifIndexN1N3 = ipv4n1->GetInterfaceForDevice(d1n3.Get(0));
  ipv4n1->SetMetric(ifIndexN1N3, n1n3Metric);

  Ptr<Ipv4> ipv4n3 = n3->GetObject<Ipv4>();
  uint32_t ifIndexN3N1 = ipv4n3->GetInterfaceForDevice(d1n3.Get(1));
  ipv4n3->SetMetric(ifIndexN3N1, n1n3Metric);

  // Populate routes
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Enable queue tracing
  AsciiTraceHelper ascii;
  p2pn0n2.EnableAsciiAll(ascii.CreateFileStream("queue-trace-n0n2.tr"));
  p2pn1n2.EnableAsciiAll(ascii.CreateFileStream("queue-trace-n1n2.tr"));
  p2pn1n3.EnableAsciiAll(ascii.CreateFileStream("queue-trace-n1n3.tr"));
  p2pn2n3.EnableAsciiAll(ascii.CreateFileStream("queue-trace-n2n3.tr"));

  // UDP Traffic: n3 -> n1
  uint16_t port = 8000;
  // Install UDP server on n1
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(n1);
  serverApp.Start(Seconds(0.5));
  serverApp.Stop(Seconds(5.0));

  // Install UDP client on n3 targeting n1's address on n1-n2 link
  UdpClientHelper client(i1n2.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp = client.Install(n3);
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(5.0));

  // Trace packet receptions
  n1->GetObject<Ipv4>()->TraceConnect("Rx", "", MakeCallback(&PacketReceiveCallback));

  // Flow Monitor for logging results
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(5.1));
  Simulator::Run();

  flowMonitor->SerializeToXmlFile("flowmon-results.xml", true, true);

  // Print UDP server summary
  uint32_t totalRx = DynamicCast<UdpServer>(serverApp.Get(0))->GetReceived();
  std::cout << "Total UDP packets received by n1: " << totalRx << std::endl;

  Simulator::Destroy();
  return 0;
}