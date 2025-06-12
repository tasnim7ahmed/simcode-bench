#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeP2PExample");

// Packet counter structs
struct PacketCounters
{
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
};

void
TxCallback(Ptr<Packet> p, const Address &a, Ptr<PacketCounters> counters)
{
  counters->txPackets++;
}

void
RxCallback(Ptr<Packet> p, const Address &a, Ptr<PacketCounters> counters)
{
  counters->rxPackets++;
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("ThreeNodeP2PExample", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(3); // n0, n1, n2

  // Connect n0 <-> n1
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  // Connect n1 <-> n2
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("5ms"));

  NetDeviceContainer d0d1 = p2p.Install(n0n1);
  NetDeviceContainer d1d2 = p2p.Install(n1n2);

  // Enable pcap tracing
  p2p.EnablePcapAll("three-node-p2p");

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

  // Set node 1 as a router (enable forwarding)
  Ptr<Ipv4> ipv4n1 = nodes.Get(1)->GetObject<Ipv4>();
  ipv4n1->SetAttribute("IpForward", BooleanValue(true));

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create Applications: n0 sends TCP to n2
  uint16_t port = 8080;
  Address sinkAddress(InetSocketAddress(i1i2.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute("DataRate", StringValue("5Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

  ApplicationContainer clientApps = onoff.Install(nodes.Get(0));

  // Set up packet counters
  std::vector<Ptr<PacketCounters>> countersList(3);
  for (uint32_t i = 0; i < 3; ++i)
    countersList[i] = CreateObject<PacketCounters>();

  // Trace TX on node 0's device 0 (n0 to n1)
  d0d1.Get(0)->TraceConnectWithoutContext(
      "PhyTxEnd",
      MakeBoundCallback(&TxCallback, countersList[0]));
  // Trace RX on node 0's device 0
  d0d1.Get(0)->TraceConnectWithoutContext(
      "PhyRxEnd",
      MakeBoundCallback(&RxCallback, countersList[0]));

  // Trace TX/RX on node 1's devices
  d0d1.Get(1)->TraceConnectWithoutContext(
      "PhyTxEnd",
      MakeBoundCallback(&TxCallback, countersList[1]));
  d0d1.Get(1)->TraceConnectWithoutContext(
      "PhyRxEnd",
      MakeBoundCallback(&RxCallback, countersList[1]));
  d1d2.Get(0)->TraceConnectWithoutContext(
      "PhyTxEnd",
      MakeBoundCallback(&TxCallback, countersList[1]));
  d1d2.Get(0)->TraceConnectWithoutContext(
      "PhyRxEnd",
      MakeBoundCallback(&RxCallback, countersList[1]));

  // Trace TX on node 2's device 0 (n2, receives from n1)
  d1d2.Get(1)->TraceConnectWithoutContext(
      "PhyTxEnd",
      MakeBoundCallback(&TxCallback, countersList[2]));
  d1d2.Get(1)->TraceConnectWithoutContext(
      "PhyRxEnd",
      MakeBoundCallback(&RxCallback, countersList[2]));

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Output packet statistics
  for (uint32_t i = 0; i < 3; ++i)
  {
    std::cout << "Node " << i << ": "
              << "TX Packets = " << countersList[i]->txPackets
              << ", RX Packets = " << countersList[i]->rxPackets
              << std::endl;
  }

  Simulator::Destroy();
  return 0;
}