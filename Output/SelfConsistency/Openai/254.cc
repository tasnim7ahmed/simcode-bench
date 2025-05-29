#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  // Enable logging for UdpEcho applications if needed
  // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install network devices on the nodes
  NetDeviceContainer devices = pointToPoint.Install (nodes);

  // Install the internet stack (TCP/IP) on the nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create and start the UDP Echo Server on node 1 (second node)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create and start the UDP Echo Client on node 0 (first node)
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // not important since only one packet
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Run the simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}