#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyExample");

void
TxTrace(Ptr<const Packet> packet)
{
  NS_LOG_INFO("Packet transmitted: UID = " << packet->GetUid() << ", Size = " << packet->GetSize());
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO("Packet received: UID = " << packet->GetUid() << ", Size = " << packet->GetSize());
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("RingTopologyExample", LOG_LEVEL_INFO);

  // Create three nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Create point-to-point links and set attributes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Connect nodes in a ring: 0-1, 1-2, 2-0
  NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer dev20 = p2p.Install(nodes.Get(2), nodes.Get(0));

  // Install the Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses to each link
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if01 = address.Assign(dev01);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if12 = address.Assign(dev12);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if20 = address.Assign(dev20);

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create and set up UDP Echo application on each node

  // Node 0 sends packet to node 1
  uint16_t port1 = 9;
  UdpEchoServerHelper echoServer1(port1);
  ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(1));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(6.0));

  UdpEchoClientHelper echoClient1(if01.GetAddress(1), port1);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(6.0));

  // Node 1 sends packet to node 2
  uint16_t port2 = 10;
  UdpEchoServerHelper echoServer2(port2);
  ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(6.0));

  UdpEchoClientHelper echoClient2(if12.GetAddress(1), port2);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(6.0));

  // Node 2 sends packet to node 0
  uint16_t port3 = 11;
  UdpEchoServerHelper echoServer3(port3);
  ApplicationContainer serverApps3 = echoServer3.Install(nodes.Get(0));
  serverApps3.Start(Seconds(1.0));
  serverApps3.Stop(Seconds(6.0));

  UdpEchoClientHelper echoClient3(if20.GetAddress(1), port3);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(2));
  clientApps3.Start(Seconds(2.0));
  clientApps3.Stop(Seconds(6.0));

  // Setup packet transmission and reception logging (tracing)
  dev01.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
  dev01.Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
  dev12.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
  dev12.Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
  dev20.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
  dev20.Get(1)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));

  dev01.Get(0)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));
  dev01.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));
  dev12.Get(0)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));
  dev12.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));
  dev20.Get(0)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));
  dev20.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));

  Simulator::Stop(Seconds(7.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}