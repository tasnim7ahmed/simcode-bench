#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int
main (int argc, char *argv[])
{
  // Configure TCP defaults
  ns3::Config::SetDefault ("ns3::TcpL4Protocol::SocketType", ns3::StringValue ("ns3::TcpNewReno"));
  ns3::Config::SetDefault ("ns3::TcpSocket::RtoMin", ns3::TimeValue (ns3::MilliSeconds (200)));

  // Create three nodes: Node 0 (Client), Node 1 (Client), Node 2 (Server)
  ns3::NodeContainer nodes;
  nodes.Create (3); // Node 0, Node 1, Node 2

  // Create PointToPointHelper for link configuration
  ns3::PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", ns3::StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));

  // Create links between nodes
  // Link 1: Node 0 <-> Node 2
  ns3::NodeContainer n0n2Nodes = ns3::NodeContainer (nodes.Get (0), nodes.Get (2));
  ns3::NetDeviceContainer d0d2Devices = pointToPoint.Install (n0n2Nodes);

  // Link 2: Node 1 <-> Node 2
  ns3::NodeContainer n1n2Nodes = ns3::NodeContainer (nodes.Get (1), nodes.Get (2));
  ns3::NetDeviceContainer d1d2Devices = pointToPoint.Install (n1n2Nodes);

  // Install the Internet stack on all nodes
  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP Addresses to the links
  ns3::Ipv4AddressHelper address;

  // Assign IP addresses for Link 1 (Node 0 - Node 2)
  address.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer i0i2Interfaces = address.Assign (d0d2Devices);
  // Node 0: i0i2Interfaces.GetAddress(0) -> 10.1.1.1
  // Node 2 (interface connected to Node 0): i0i2Interfaces.GetAddress(1) -> 10.1.1.2
  ns3::Ipv4Address serverAddressForClient0 = i0i2Interfaces.GetAddress (1);

  // Assign IP addresses for Link 2 (Node 1 - Node 2)
  address.SetBase ("10.1.2.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer i1i2Interfaces = address.Assign (d1d2Devices);
  // Node 1: i1i2Interfaces.GetAddress(0) -> 10.1.2.1
  // Node 2 (interface connected to Node 1): i1i2Interfaces.GetAddress(1) -> 10.1.2.2
  ns3::Ipv4Address serverAddressForClient1 = i1i2Interfaces.GetAddress (1);

  // Configure TCP Server on Node 2 (listens on port 9)
  uint16_t serverPort = 9;
  ns3::PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                          ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), serverPort));
  ns3::ApplicationContainer serverApps = packetSinkHelper.Install (nodes.Get (2));
  serverApps.Start (ns3::Seconds (0.0));
  serverApps.Stop (ns3::Seconds (10.0));

  // Configure TCP Clients
  uint32_t payloadSize = 1460; // Typical TCP payload size
  ns3::OnOffHelper onoff ("ns3::TcpSocketFactory", ns3::Address ()); // Address will be set later

  // Client on Node 0
  onoff.SetAttribute ("OnTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=1]")); // Always On
  onoff.SetAttribute ("OffTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=0]")); // Never Off
  onoff.SetAttribute ("DataRate", ns3::StringValue ("1Mbps")); // Maximize traffic for the link
  onoff.SetAttribute ("PacketSize", ns3::UintegerValue (payloadSize));

  // Client 0 connects to Node 2's interface on the 10.1.1.0/24 network
  ns3::Address clientAddress0 = ns3::InetSocketAddress (serverAddressForClient0, serverPort);
  onoff.SetAttribute ("Remote", ns3::AddressValue (clientAddress0));
  ns3::ApplicationContainer clientApps0 = onoff.Install (nodes.Get (0));
  clientApps0.Start (ns3::Seconds (2.0)); // Client 0 starts at 2 seconds
  clientApps0.Stop (ns3::Seconds (10.0));

  // Client on Node 1
  // Re-use onoff helper, just update remote address
  // Client 1 connects to Node 2's interface on the 10.1.2.0/24 network
  ns3::Address clientAddress1 = ns3::InetSocketAddress (serverAddressForClient1, serverPort);
  onoff.SetAttribute ("Remote", ns3::AddressValue (clientAddress1));
  ns3::ApplicationContainer clientApps1 = onoff.Install (nodes.Get (1));
  clientApps1.Start (ns3::Seconds (3.0)); // Client 1 starts at 3 seconds
  clientApps1.Stop (ns3::Seconds (10.0));

  // Populate routing tables (essential for multi-homed Node 2)
  ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set the simulation stop time
  ns3::Simulator::Stop (ns3::Seconds (10.0));

  // Run the simulation
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}