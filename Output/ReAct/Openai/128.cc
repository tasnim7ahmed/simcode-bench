#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ospf-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfLinkStateRoutingExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Four routers
  NodeContainer routers;
  routers.Create (4);

  // Setup point-to-point channels between routers in a mesh
  // R0-R1, R0-R2, R1-R2, R1-R3, R2-R3
  NodeContainer n0n1 = NodeContainer (routers.Get (0), routers.Get (1));
  NodeContainer n0n2 = NodeContainer (routers.Get (0), routers.Get (2));
  NodeContainer n1n2 = NodeContainer (routers.Get (1), routers.Get (2));
  NodeContainer n1n3 = NodeContainer (routers.Get (1), routers.Get (3));
  NodeContainer n2n3 = NodeContainer (routers.Get (2), routers.Get (3));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = p2p.Install (n0n1);
  NetDeviceContainer d0d2 = p2p.Install (n0n2);
  NetDeviceContainer d1d2 = p2p.Install (n1n2);
  NetDeviceContainer d1d3 = p2p.Install (n1n3);
  NetDeviceContainer d2d3 = p2p.Install (n2n3);

  // Install Internet stack with OSPF routing
  OspfRoutingHelper ospf;
  InternetStackHelper stack;
  stack.SetRoutingHelper (ospf);
  stack.Install (routers);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = address.Assign (d0d2);

  address.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  address.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i3 = address.Assign (d1d3);

  address.SetBase ("10.0.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign (d2d3);

  // Enable pcap tracing on all router devices to see LSAs and other OSPF packets
  p2p.EnablePcapAll ("ospf-lsa");

  // Let OSPF converge
  Simulator::Schedule (Seconds (1.0), [] () {
    NS_LOG_UNCOND ("OSPF convergence and LSAs should now be exchanged");
  });

  // Application: UDP flow from R0 to R3
  uint16_t port = 50000;
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
    InetSocketAddress (Ipv4Address ("10.0.5.2"), port));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (15.0)));

  // For UDP sink on R3
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
    InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (routers.Get (3));
  sinkApps.Start (Seconds (1.99));
  sinkApps.Stop (Seconds (16.0));

  ApplicationContainer app = onoff.Install (routers.Get (0));

  // Simulate a link failure (down n1-n3 at 7s to trigger new LSA and OSPF update)
  Simulator::Schedule (Seconds (7.0), [&](){
    NS_LOG_UNCOND ("*** Bringing link n1-n3 DOWN at 7s ***");
    d1d3.Get (0)->SetDown ();
    d1d3.Get (1)->SetDown ();
  });

  // Restore the link at 11s
  Simulator::Schedule (Seconds (11.0), [&](){
    NS_LOG_UNCOND ("*** Bringing link n1-n3 UP at 11s ***");
    d1d3.Get (0)->SetUp ();
    d1d3.Get (1)->SetUp ();
  });

  Simulator::Stop (Seconds (16.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}