#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

void
TxTrace(Ptr<const Packet> packet)
{
  NS_LOG_INFO("Packet transmitted at " << Simulator::Now().GetSeconds() << "s, size: " << packet->GetSize() << " bytes");
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO("Packet received at " << Simulator::Now().GetSeconds() << "s, size: " << packet->GetSize() << " bytes");
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("StarTopologySimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3);

  // Setup point-to-point helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("1ms"));

  // Connect node 0 (center) to node 1
  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d0d1 = p2p.Install(n0n1);

  // Connect node 0 (center) to node 2
  NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
  NetDeviceContainer d0d2 = p2p.Install(n0n2);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Setup UDP Echo server on node 2
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Setup UDP Echo client on node 1 targeting node 2 IP address
  UdpEchoClientHelper echoClient(i0i2.GetAddress(1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Connect trace sources for logging
  d0d1.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
  d0d1.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));
  d0d2.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
  d0d2.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}