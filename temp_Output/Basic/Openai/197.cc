#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

uint32_t txPackets[3] = {0};
uint32_t rxPackets[3] = {0};

void
TxTrace(uint32_t nodeId, Ptr<const Packet> pkt)
{
  txPackets[nodeId]++;
}

void
RxTrace(uint32_t nodeId, Ptr<const Packet> pkt, const Address &address)
{
  rxPackets[nodeId]++;
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  NodeContainer nodes;
  nodes.Create(3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer dev1 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer dev2 = p2p.Install(nodes.Get(1), nodes.Get(2));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface1 = address.Assign(dev1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface2 = address.Assign(dev2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Packet tracing: each device's MAC tx and node rx (via protocol stack)
  dev1.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback(&TxTrace, 0));
  dev1.Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback(&TxTrace, 1));
  dev2.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback(&TxTrace, 1));
  dev2.Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeBoundCallback(&TxTrace, 2));

  Ptr<Node> n0 = nodes.Get(0);
  Ptr<Node> n1 = nodes.Get(1);
  Ptr<Node> n2 = nodes.Get(2);

  n0->GetObject<Ipv4L3Protocol>()->TraceConnectWithoutContext("Receive", MakeBoundCallback(&RxTrace, 0));
  n1->GetObject<Ipv4L3Protocol>()->TraceConnectWithoutContext("Receive", MakeBoundCallback(&RxTrace, 1));
  n2->GetObject<Ipv4L3Protocol>()->TraceConnectWithoutContext("Receive", MakeBoundCallback(&RxTrace, 2));

  // Enable pcap
  p2p.EnablePcapAll("three-nodes-p2p");

  // Install TCP receiver on node 2
  uint16_t port = 8080;
  Address sinkAddress(InetSocketAddress(iface2.GetAddress(1), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Install TCP sender on node 0
  OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

  ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  std::cout << "Tx packets per node:" << std::endl;
  for (uint32_t i = 0; i < 3; ++i)
    std::cout << "  Node " << i << ": " << txPackets[i] << std::endl;

  std::cout << "Rx packets per node:" << std::endl;
  for (uint32_t i = 0; i < 3; ++i)
    std::cout << "  Node " << i << ": " << rxPackets[i] << std::endl;

  Simulator::Destroy();
  return 0;
}