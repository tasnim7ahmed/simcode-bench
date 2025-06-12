/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExample");

int
main(int argc, char *argv[])
{
  // Enable logging for the OnOffApplication and PacketSink
  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  // Create 4 nodes: n0 (root), n1 (child), n2 (child), n3 (leaf)
  NodeContainer nodes;
  nodes.Create(4);

  // Assign names for clarity
  Ptr<Node> n0 = nodes.Get(0); // root
  Ptr<Node> n1 = nodes.Get(1); // child1
  Ptr<Node> n2 = nodes.Get(2); // child2 (will connect to the leaf)
  Ptr<Node> n3 = nodes.Get(3); // leaf

  // Setup point-to-point parameters
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Create net device containers for each link
  // n0 - n1
  NodeContainer n0n1(n0, n1);
  NetDeviceContainer d0d1 = p2p.Install(n0n1);

  // n0 - n2
  NodeContainer n0n2(n0, n2);
  NetDeviceContainer d0d2 = p2p.Install(n0n2);

  // n2 - n3
  NodeContainer n2n3(n2, n3);
  NetDeviceContainer d2d3 = p2p.Install(n2n3);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install a packet sink on the root node (n0)
  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(i0i1.GetAddress(0), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(n0);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Install an OnOffApplication on the leaf node (n3) to send data to n0
  OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = onoff.Install(n3);
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(9.0));

  // Enable pcap tracing
  p2p.EnablePcapAll("tree-topology");

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}