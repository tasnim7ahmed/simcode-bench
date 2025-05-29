/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * An ns-3 simulation demonstrating OSPF routing using FSR as a stand-in, 
 * since ns-3 does not support OSPF natively. The simulation uses Ipv4GlobalRouting,
 * which calculates shortest paths as a global routing table (analogous to how OSPF would).
 * NetAnim visualizes the topology and packet flows.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfDemo");

int
main (int argc, char *argv[])
{
  // Configure simulation parameters
  uint32_t nNodes = 5;
  double simulationTime = 15.0;

  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of nodes in the ring topology", nNodes);
  cmd.AddValue ("simulationTime", "Simulation time (seconds)", simulationTime);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (nNodes);

  // Install Internet stack with Ipv4GlobalRouting (shortest path routing)
  InternetStackHelper internet;
  Ipv4GlobalRoutingHelper globalRouting;
  internet.SetRoutingHelper (globalRouting);
  internet.Install (nodes);

  // Create point-to-point links forming a ring topology
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  std::vector<NetDeviceContainer> devices;
  std::vector<Ipv4InterfaceContainer> interfaces;

  Ipv4AddressHelper ipv4;
  uint32_t subnetCount = 0;

  for (uint32_t i = 0; i < nNodes; ++i)
    {
      uint32_t next = (i + 1) % nNodes;
      NetDeviceContainer link = p2p.Install (nodes.Get (i), nodes.Get (next));
      devices.push_back (link);

      std::ostringstream subnet;
      subnet << "10.1." << subnetCount++ << ".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      Ipv4InterfaceContainer iface = ipv4.Assign (link);
      interfaces.push_back (iface);
    }

  // Create a UDP flow from node 0 to node nNodes/2 (across the ring)
  uint16_t port = 9;
  uint32_t dstNodeIndex = nNodes / 2;

  // UDP server on receiver
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (dstNodeIndex));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  // UDP client sends packets to receiver via shortest path
  Ipv4Address dstAddr = interfaces[(dstNodeIndex - 1 + nNodes) % nNodes].GetAddress (1);
  UdpClientHelper client (dstAddr, port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  client.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 1));

  // Enable NetAnim tracing
  AnimationInterface anim ("ospf-simulation.xml");

  // Assign node positions for better visualization
  double radius = 50.0;
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      double angle = 2 * M_PI * i / nNodes;
      double x = 100.0 + radius * std::cos (angle);
      double y = 100.0 + radius * std::sin (angle);
      anim.SetConstantPosition (nodes.Get (i), x, y);
    }

  // Enable packet capture on all links for further analysis (optional)
  for (size_t i = 0; i < devices.size (); ++i)
    {
      p2p.EnablePcap ("ospf-node-" + std::to_string (i), devices[i], true);
    }

  // Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}