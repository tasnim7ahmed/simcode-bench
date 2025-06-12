#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include <fstream>

using namespace ns3;

static void
ReceivePacket(Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream recvStream("packet-receptions.txt", std::ios_base::app);
  recvStream << "Received packet of size " << packet->GetSize()
             << " bytes at " << Simulator::Now().GetSeconds() << " s" << std::endl;
}

static void
QueueTrace(Ptr<QueueDisc> queue)
{
  static std::ofstream queueStream("queue-traces.txt", std::ios_base::app);
  queueStream << "At " << Simulator::Now().GetSeconds()
              << " s: queue size = " << queue->GetNPackets() << std::endl;
  Simulator::Schedule(Seconds(0.05), &QueueTrace, queue);
}

int
main(int argc, char *argv[])
{
  // Create nodes
  NodeContainer n0n2, n1n2, csmaNodes, n5n6;
  Ptr<Node> n0 = CreateObject<Node>();
  Ptr<Node> n1 = CreateObject<Node>();
  Ptr<Node> n2 = CreateObject<Node>();
  Ptr<Node> n3 = CreateObject<Node>();
  Ptr<Node> n4 = CreateObject<Node>();
  Ptr<Node> n5 = CreateObject<Node>();
  Ptr<Node> n6 = CreateObject<Node>();

  n0n2.Add(n0);
  n0n2.Add(n2);

  n1n2.Add(n1);
  n1n2.Add(n2);

  csmaNodes.Add(n2);
  csmaNodes.Add(n3);
  csmaNodes.Add(n4);
  csmaNodes.Add(n5);

  n5n6.Add(n5);
  n5n6.Add(n6);

  // Install Internet stack
  NodeContainer allNodes;
  allNodes.Add(n0);
  allNodes.Add(n1);
  allNodes.Add(n2);
  allNodes.Add(n3);
  allNodes.Add(n4);
  allNodes.Add(n5);
  allNodes.Add(n6);

  InternetStackHelper internet;
  internet.Install(allNodes);

  // Point-to-point links: n0-n2, n1-n2, n5-n6
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d0n2 = p2p.Install(n0n2);
  NetDeviceContainer d1n2 = p2p.Install(n1n2);
  NetDeviceContainer d5n6 = p2p.Install(n5n6);

  // CSMA segment: n2, n3, n4, n5
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0n2 = ipv4.Assign(d0n2);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1n2 = ipv4.Assign(d1n2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer icsma = ipv4.Assign(csmaDevices);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i5n6 = ipv4.Assign(d5n6);

  // CBR/UDP flow from n0 to n6
  uint16_t port = 4000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(n6);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(20.0));

  // OnOff application on n0
  OnOffHelper onoff("ns3::UdpSocketFactory",
                    InetSocketAddress(i5n6.GetAddress(1), port)); // n6 IP
  onoff.SetAttribute("PacketSize", UintegerValue(50));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("300bps")));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(19.0)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.Install(n0);

  // Trace packet receptions at n6
  Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
  sink->TraceConnectWithoutContext("Rx", MakeCallback(&ReceivePacket));

  // Trace all network queues (use traffic control queue on point-to-point devices)
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
  QueueDiscContainer qd0n2 = tch.Install(d0n2);
  QueueDiscContainer qd1n2 = tch.Install(d1n2);
  QueueDiscContainer qd5n6 = tch.Install(d5n6);

  Simulator::Schedule(Seconds(0.0), &QueueTrace, qd0n2.Get(0));
  Simulator::Schedule(Seconds(0.0), &QueueTrace, qd1n2.Get(0));
  Simulator::Schedule(Seconds(0.0), &QueueTrace, qd5n6.Get(0));

  // Enable pcap tracing
  p2p.EnablePcapAll("p2p");
  csma.EnablePcapAll("csma", true);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}