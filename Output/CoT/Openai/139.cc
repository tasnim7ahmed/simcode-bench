#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpEchoSixNodeExample");

void TxCallback (Ptr<const Packet> packet)
{
  NS_LOG_INFO ("Packet transmitted at " << Simulator::Now ().GetSeconds () << "s, size: " << packet->GetSize ());
}

void RxCallback (Ptr<const Packet> packet, const Address &)
{
  NS_LOG_INFO ("Packet received at " << Simulator::Now ().GetSeconds () << "s, size: " << packet->GetSize ());
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoSixNodeExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (6);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create and install net devices
  NetDeviceContainer dev01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer dev12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer dev23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer dev34 = p2p.Install (nodes.Get (3), nodes.Get (4));
  NetDeviceContainer dev45 = p2p.Install (nodes.Get (4), nodes.Get (5));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces.push_back (address.Assign (dev01));
  address.SetBase ("10.1.2.0", "255.255.255.0");
  interfaces.push_back (address.Assign (dev12));
  address.SetBase ("10.1.3.0", "255.255.255.0");
  interfaces.push_back (address.Assign (dev23));
  address.SetBase ("10.1.4.0", "255.255.255.0");
  interfaces.push_back (address.Assign (dev34));
  address.SetBase ("10.1.5.0", "255.255.255.0");
  interfaces.push_back (address.Assign (dev45));

  // Enable static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo Server on node 5
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApp = echoServer.Install (nodes.Get (5));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // UDP Echo Client on node 0 -> server address is 10.1.5.2
  Ipv4Address serverAddress = interfaces[4].GetAddress (1);
  UdpEchoClientHelper echoClient (serverAddress, echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApp = echoClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Trace packet transmission and reception
  dev01.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxCallback));
  dev01.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxCallback));
  dev12.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxCallback));
  dev12.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxCallback));
  dev23.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxCallback));
  dev23.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxCallback));
  dev34.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxCallback));
  dev34.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxCallback));
  dev45.Get (0)->TraceConnectWithoutContext ("PhyTxEnd", MakeCallback (&TxCallback));
  dev45.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&RxCallback));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}