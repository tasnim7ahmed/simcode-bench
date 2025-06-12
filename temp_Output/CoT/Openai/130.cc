#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-nat-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NatSimulation");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("NatSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // Nodes: 2 private hosts, 1 NAT router, 1 public server
  NodeContainer privateHosts;
  privateHosts.Create (2);

  Ptr<Node> natRouter = CreateObject<Node> ();
  Ptr<Node> publicServer = CreateObject<Node> ();

  // Point-to-Point links
  PointToPointHelper p2pPriv;
  p2pPriv.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pPriv.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer privToNat1 = p2pPriv.Install (privateHosts.Get (0), natRouter);
  NetDeviceContainer privToNat2 = p2pPriv.Install (privateHosts.Get (1), natRouter);

  PointToPointHelper p2pPub;
  p2pPub.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pPub.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer natToPub = p2pPub.Install (natRouter, publicServer);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (privateHosts);
  internet.Install (natRouter);
  internet.Install (publicServer);

  // IP addressing
  Ipv4AddressHelper privSubnet1;
  privSubnet1.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifacePriv1 = privSubnet1.Assign (privToNat1);

  Ipv4AddressHelper privSubnet2;
  privSubnet2.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifacePriv2 = privSubnet2.Assign (privToNat2);

  Ipv4AddressHelper pubSubnet;
  pubSubnet.SetBase ("198.51.100.0", "255.255.255.0");
  Ipv4InterfaceContainer ifacePub = pubSubnet.Assign (natToPub);

  Ipv4InterfaceContainer ifaceServer;
  ifaceServer.Add (ifacePub.Get (1)); // Public server

  // Enable IP forwarding on NAT router
  Ptr<Ipv4> ipv4Nat = natRouter->GetObject<Ipv4> ();
  ipv4Nat->SetAttribute ("IpForward", BooleanValue (true));

  // Setup NAT
  Ipv4NatHelper natHelper;
  Ptr<Ipv4Nat> theNat = natHelper.Install (natRouter);

  // Add inside interfaces (the two private subnets)
  theNat->SetAttribute ("DefaultInsideAddress", Ipv4AddressValue ("10.0.0.1")); // Not used directly here
  theNat->AddNatInside (ifacePriv1.Get (1), ifacePriv1.GetAddress (1));
  theNat->AddNatInside (ifacePriv2.Get (1), ifacePriv2.GetAddress (1));

  // Add outside interface (the public internet side)
  theNat->AddNatOutside (ifacePub.Get (0), ifacePub.GetAddress (0));

  // Setup static routes on private hosts to route via NAT router
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4StaticRouting> host1StaticRouting = staticRouting.GetStaticRouting (privateHosts.Get (0)->GetObject<Ipv4> ());
  host1StaticRouting->SetDefaultRoute (ifacePriv1.GetAddress (1), 1);

  Ptr<Ipv4StaticRouting> host2StaticRouting = staticRouting.GetStaticRouting (privateHosts.Get (1)->GetObject<Ipv4> ());
  host2StaticRouting->SetDefaultRoute (ifacePriv2.GetAddress (1), 1);

  // Public server default route not necessary, it has only one interface

  // UDP server on public server
  uint16_t serverPort = 4000;
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApps = udpServer.Install (publicServer);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP clients on private hosts
  UdpClientHelper udpClient (ifaceServer.GetAddress (0), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (5));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (64));

  ApplicationContainer clientApps;
  clientApps.Add (udpClient.Install (privateHosts.Get (0)));
  clientApps.Add (udpClient.Install (privateHosts.Get (1)));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (9.0));

  // Allow responses to go through NAT
  theNat->AddRule (Ipv4Nat::PROTO_UDP,
                   ifaceServer.GetAddress (0), serverPort,
                   Ipv4Address ("10.0.1.1"), 40000, // placeholder for all source ports
                   Ipv4Address ("0.0.0.0"), 0,      // translated to NAT public
                   Ipv4Address ("10.0.1.1"), 0);    // original destination

  // Enable pcap on relevant devices
  p2pPriv.EnablePcap ("host1-nat", privToNat1.Get (0), true);
  p2pPriv.EnablePcap ("host2-nat", privToNat2.Get (0), true);
  p2pPriv.EnablePcap ("nat-router-inside1", privToNat1.Get (1), true);
  p2pPriv.EnablePcap ("nat-router-inside2", privToNat2.Get (1), true);
  p2pPub.EnablePcap ("nat-router-outside", natToPub.Get (0), true);
  p2pPub.EnablePcap ("public-server", natToPub.Get (1), true);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}