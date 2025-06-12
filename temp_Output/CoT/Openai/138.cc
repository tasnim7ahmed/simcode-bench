#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FourNodeUdpEcho");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  // Link 0-1
  NodeContainer n0n1 = NodeContainer (nodes.Get(0), nodes.Get(1));
  // Link 1-2
  NodeContainer n1n2 = NodeContainer (nodes.Get(1), nodes.Get(2));
  // Link 2-3
  NodeContainer n2n3 = NodeContainer (nodes.Get(2), nodes.Get(3));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = pointToPoint.Install (n0n1);
  NetDeviceContainer d1d2 = pointToPoint.Install (n1n2);
  NetDeviceContainer d2d3 = pointToPoint.Install (n2n3);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign (d2d3);

  // Enable Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo Server on node 3, listening on port 9
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on node 0, sending to server's address through the three links
  UdpEchoClientHelper echoClient (i2i3.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable pcap tracing on all links
  pointToPoint.EnablePcapAll ("four-node-udp-echo");

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}