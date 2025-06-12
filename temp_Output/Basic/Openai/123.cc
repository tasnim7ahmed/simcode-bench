#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-ospf-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4); // Node0, Node1, Node2, Node3

  // Square topology:
  // Node0----Node1
  //   |        |
  // Node3----Node2

  // Create links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d01 = p2p.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d12 = p2p.Install (nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d23 = p2p.Install (nodes.Get(2), nodes.Get(3));
  NetDeviceContainer d30 = p2p.Install (nodes.Get(3), nodes.Get(0));

  // Assign IP addresses
  InternetStackHelper internet;
  Ipv4OspfRoutingHelper ospf;
  internet.SetRoutingHelper (ospf);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = address.Assign (d01);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = address.Assign (d12);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = address.Assign (d23);
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i30 = address.Assign (d30);

  // Create pcap tracing for each link
  p2p.EnablePcap ("square_p2p-01", d01, true);
  p2p.EnablePcap ("square_p2p-12", d12, true);
  p2p.EnablePcap ("square_p2p-23", d23, true);
  p2p.EnablePcap ("square_p2p-30", d30, true);

  // Set up a UDP server on Node2
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get(2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Set up a UDP client on Node0, talking to Node2
  UdpEchoClientHelper echoClient (i12.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Run simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  // Print the routing tables at the end of the simulation
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      ospf.PrintRoutingTable (ipv4, routingStream);
    }

  Simulator::Destroy ();
  return 0;
}