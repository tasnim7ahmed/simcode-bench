#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-nat-helper.h"
#include "ns3/ipv4-nat.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Nodes: host1, host2, natRouter, publicServer
  NodeContainer privateHosts;
  privateHosts.Create (2); // host1, host2
  Ptr<Node> host1 = privateHosts.Get (0);
  Ptr<Node> host2 = privateHosts.Get (1);

  Ptr<Node> natRouter = CreateObject<Node> ();
  Ptr<Node> publicServer = CreateObject<Node> ();

  // Private network: [host1]---+                +---[publicServer]
  //                           |---[natRouter]---|
  //             [host2]-------+

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // host1 <-> natRouter
  NetDeviceContainer d_host1_nat = p2p.Install (host1, natRouter);

  // host2 <-> natRouter
  NetDeviceContainer d_host2_nat = p2p.Install (host2, natRouter);

  // natRouter <-> publicServer
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer d_nat_public = p2p.Install (natRouter, publicServer);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (privateHosts);
  internet.Install (natRouter);
  internet.Install (publicServer);

  // Assign IP addresses
  Ipv4AddressHelper address;
  // Private subnet 10.0.1.0/24
  address.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_host1_nat = address.Assign (d_host1_nat);
  // 10.0.1.1 (host1), 10.0.1.2 (nat private side)
  address.NewNetwork ();
  Ipv4InterfaceContainer if_host2_nat = address.Assign (d_host2_nat);
  // 10.0.2.1 (host2), 10.0.2.2 (nat private side)
  // Public subnet 203.0.113.0/24
  address.SetBase ("203.0.113.0", "255.255.255.0");
  Ipv4InterfaceContainer if_nat_public = address.Assign (d_nat_public);
  // 203.0.113.1 (nat public side), 203.0.113.2 (publicServer)

  // Configure NAT on natRouter
  Ipv4NatHelper natHelper;
  Ptr<Ipv4Nat> nat = natHelper.Install (natRouter);

  // Add inside and outside interfaces to the NAT
  // Inside: host1-natRouter (if index 1), host2-natRouter (if index 2)
  // Outside: natRouter-publicServer (if index 3)
  // Sanity check interface ordering
  uint32_t ifcHost1Nat = 1; // net-device 1
  uint32_t ifcHost2Nat = 2; // net-device 2
  uint32_t ifcNatPublic = 3; // net-device 3
  nat->SetAttribute ("InsideInterfaces", Ipv4InterfaceIdValue (ifcHost1Nat));
  nat->AddInsideInterface (ifcHost2Nat);
  nat->SetAttribute ("OutsideInterface", Ipv4InterfaceIdValue (ifcNatPublic));

  // Assign a public address pool for NAT translation (203.0.113.1, the natRouter's public IP)
  nat->AddAddressPool (Ipv4Address ("203.0.113.1"), Ipv4Mask ("255.255.255.255"));

  // Enable forwarding on NAT
  Ptr<Ipv4> ipv4Nat = natRouter->GetObject<Ipv4> ();
  ipv4Nat->SetAttribute ("IpForward", BooleanValue (true));

  // Enable forwarding on public server in case it's needed
  Ptr<Ipv4> ipv4PublicSvr = publicServer->GetObject<Ipv4> ();
  ipv4PublicSvr->SetAttribute ("IpForward", BooleanValue (true));

  // Routing: Private hosts default route via natRouter
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> h1StaticRt = ipv4RoutingHelper.GetStaticRouting (host1->GetObject<Ipv4> ());
  h1StaticRt->SetDefaultRoute (if_host1_nat.GetAddress (1), 1);

  Ptr<Ipv4StaticRouting> h2StaticRt = ipv4RoutingHelper.GetStaticRouting (host2->GetObject<Ipv4> ());
  h2StaticRt->SetDefaultRoute (if_host2_nat.GetAddress (1), 1);

  // NAT router: route between all subnets as needed
  Ptr<Ipv4StaticRouting> natStaticRt = ipv4RoutingHelper.GetStaticRouting (natRouter->GetObject<Ipv4> ());
  // Knowing that NAT takes care of most, but for completeness, let NAT access publicServer directly
  natStaticRt->AddNetworkRouteTo (Ipv4Address ("203.0.113.0"), Ipv4Mask ("255.255.255.0"), if_nat_public.GetAddress (1), ifcNatPublic);

  // publicServer: route to private networks via natRouter
  Ptr<Ipv4StaticRouting> svrStaticRt = ipv4RoutingHelper.GetStaticRouting (publicServer->GetObject<Ipv4> ());
  svrStaticRt->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"), if_nat_public.GetAddress (0), 1);
  svrStaticRt->AddNetworkRouteTo (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"), if_nat_public.GetAddress (0), 1);

  // Install UDP server on publicServer
  uint16_t udpPort = 40000;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApp = udpServer.Install (publicServer);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (15.0));

  // Install UDP clients on host1 and host2 targeting publicServer
  uint32_t maxPackets = 2;
  Time interPacketInterval = Seconds (1.0);

  UdpClientHelper udpClient1 (if_nat_public.GetAddress (1), udpPort);
  udpClient1.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  udpClient1.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient1.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApp1 = udpClient1.Install (host1);
  clientApp1.Start (Seconds (1.0));
  clientApp1.Stop (Seconds (10.0));

  UdpClientHelper udpClient2 (if_nat_public.GetAddress (1), udpPort);
  udpClient2.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  udpClient2.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApp2 = udpClient2.Install (host2);
  clientApp2.Start (Seconds (1.5));
  clientApp2.Stop (Seconds (10.0));

  // Enable PCAP tracing
  p2p.EnablePcapAll ("nat-sim", true);

  Simulator::Stop (Seconds (15.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}