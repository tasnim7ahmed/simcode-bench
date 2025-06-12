#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Create three nodes: Router A, Router B, Router C
  NodeContainer routers;
  routers.Create (3);

  Ptr<Node> routerA = routers.Get (0);
  Ptr<Node> routerB = routers.Get (1);
  Ptr<Node> routerC = routers.Get (2);

  // Point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Link A <-> B
  NodeContainer ab (routerA, routerB);
  NetDeviceContainer abDevs = p2p.Install (ab);

  // Link B <-> C
  NodeContainer bc (routerB, routerC);
  NetDeviceContainer bcDevs = p2p.Install (bc);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (routers);

  // Assign IP addresses
  // A <-> B : x.x.x.0/30
  Ipv4AddressHelper ipv4_ab;
  ipv4_ab.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer abIfaces = ipv4_ab.Assign (abDevs);

  // B <-> C : y.y.y.0/30
  Ipv4AddressHelper ipv4_bc;
  ipv4_bc.SetBase ("10.2.2.0", "255.255.255.252");
  Ipv4InterfaceContainer bcIfaces = ipv4_bc.Assign (bcDevs);

  // Loopback for router A (a.a.a.a/32)
  Ptr<Ipv4> ipv4A = routerA->GetObject<Ipv4> ();
  int32_t loIfA = ipv4A->GetInterfaceForDevice (routerA->GetDevice (0));
  Ipv4InterfaceAddress addrA = Ipv4InterfaceAddress (Ipv4Address ("1.1.1.1"), Ipv4Mask ("255.255.255.255"));
  ipv4A->AddAddress (1, addrA);

  // Loopback for router C (c.c.c.c/32)
  Ptr<Ipv4> ipv4C = routerC->GetObject<Ipv4> ();
  int32_t loIfC = ipv4C->GetInterfaceForDevice (routerC->GetDevice (0));
  Ipv4InterfaceAddress addrC = Ipv4InterfaceAddress (Ipv4Address ("3.3.3.3"), Ipv4Mask ("255.255.255.255"));
  ipv4C->AddAddress (1, addrC);

  // Enable IP forwarding (routers have no NS-3 forwarder unless global routing/forwarding is enabled)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Setup UDP PacketSink on router C (c.c.c.c:9999)
  uint16_t sinkPort = 9999;
  Address sinkAddress (InetSocketAddress (Ipv4Address ("3.3.3.3"), sinkPort));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (routerC);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Setup OnOffApplication on router A to send UDP to router C's loopback (c.c.c.c:9999)
  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));
  onoff.SetAttribute ("Remote", AddressValue (sinkAddress));
  ApplicationContainer app = onoff.Install (routerA);

  // Enable packet tracing
  p2p.EnablePcapAll ("three-router");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}