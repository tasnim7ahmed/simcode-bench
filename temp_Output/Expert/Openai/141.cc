#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologySimulation");

void TxTrace (Ptr<const Packet> packet)
{
  NS_LOG_INFO ("Packet transmitted: UID=" << packet->GetUid ());
}

void RxTrace (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO ("Packet received: UID=" << packet->GetUid ());
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("RingTopologySimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dev01, dev12, dev20;
  dev01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  dev12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  dev20 = p2p.Install (nodes.Get (2), nodes.Get (0));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  Ipv4InterfaceContainer if01, if12, if20;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  if01 = ipv4.Assign (dev01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  if12 = ipv4.Assign (dev12);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  if20 = ipv4.Assign (dev20);

  uint16_t port = 8080;
  ApplicationContainer serverApps, clientApps;

  // Install UDP server on node 1
  UdpServerHelper server (port);
  serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (16.0));

  // UDP client on node 0 sends to server (node 1) across the ring
  UdpClientHelper client0 (if01.GetAddress (1), port);
  client0.SetAttribute ("MaxPackets", UintegerValue (5));
  client0.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client0.SetAttribute ("PacketSize", UintegerValue (1024));
  clientApps.Add (client0.Install (nodes.Get (0)));

  // UDP client on node 2 sends to server (node 1)
  UdpClientHelper client2 (if12.GetAddress (0), port);
  client2.SetAttribute ("MaxPackets", UintegerValue (5));
  client2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client2.SetAttribute ("PacketSize", UintegerValue (1024));
  clientApps.Add (client2.Install (nodes.Get (2)));

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (15.0));

  // Connect trace sources for transmission and reception logging
  dev01.Get (0)->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&TxTrace));
  dev01.Get (1)->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&TxTrace));
  dev12.Get (0)->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&TxTrace));
  dev12.Get (1)->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&TxTrace));
  dev20.Get (0)->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&TxTrace));
  dev20.Get (1)->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&TxTrace));

  dev01.Get (0)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));
  dev01.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));
  dev12.Get (0)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));
  dev12.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));
  dev20.Get (0)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));
  dev20.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxTrace));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (17.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}