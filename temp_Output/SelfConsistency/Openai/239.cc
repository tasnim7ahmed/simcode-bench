/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpRouterExample");

int
main (int argc, char *argv[])
{
  // Enable logging (optional)
  // LogComponentEnable ("TcpRouterExample", LOG_LEVEL_INFO);

  // Set simulation parameters
  double simTime = 10.0;

  // Create three nodes: client, router, server
  NodeContainer nodes;
  nodes.Create (3);

  // Point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Link client <-> router
  NodeContainer clientRouter = NodeContainer (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer devClientRouter = p2p.Install (clientRouter);

  // Link router <-> server
  NodeContainer routerServer = NodeContainer (nodes.Get(1), nodes.Get(2));
  NetDeviceContainer devRouterServer = p2p.Install (routerServer);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifClientRouter = address.Assign (devClientRouter);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifRouterServer = address.Assign (devRouterServer);

  // Set up routing - let ns-3 do it automatically
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install TCP server on node 2 (rightmost node)
  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress(ifRouterServer.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get(2));
  serverApp.Start (Seconds(0.0));
  serverApp.Stop (Seconds(simTime));

  // Install TCP client on node 0 (leftmost node)
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds(1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds(simTime)));

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get(0));

  // Enable traces (optional)
  // p2p.EnablePcapAll ("tcp-router-p2p");

  Simulator::Stop (Seconds (simTime+1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}