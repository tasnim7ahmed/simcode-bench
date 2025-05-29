#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // 0: client, 1: router, 2: server

  // Point-to-Point links
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1 = p2p1.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devices2 = p2p2.Install (nodes.Get(1), nodes.Get(2));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP Addresses
  Ipv4AddressHelper address1;
  address1.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address1.Assign (devices1);

  Ipv4AddressHelper address2;
  address2.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address2.Assign (devices2);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // TCP Server (PacketSink) on node 2
  uint16_t port = 8080;
  Address sinkAddress (InetSocketAddress (interfaces2.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get(2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (20.0));

  // TCP Client (OnOffApplication) on node 0
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get(0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (19.0));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}