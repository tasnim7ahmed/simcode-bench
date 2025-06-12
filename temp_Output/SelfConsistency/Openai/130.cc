/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-nat-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NATPrivateNetworkSimulation");

void
ServerRxTrace (Ptr<Packet> packet, const Address &src)
{
  Ipv4Header ipHeader;
  Ptr<Packet> pCopy = packet->Copy();
  pCopy->RemoveHeader(ipHeader);
  NS_LOG_INFO ("Server received packet from: " << ipHeader.GetSource());
}

void
HostRxTrace (Ptr<Packet> packet, const Address &src)
{
  Ipv4Header ipHeader;
  Ptr<Packet> pCopy = packet->Copy();
  pCopy->RemoveHeader(ipHeader);
  NS_LOG_INFO ("Host received packet from: " << ipHeader.GetSource());
}

int
main (int argc, char *argv[])
{
  LogComponentEnable("NATPrivateNetworkSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Topology:
  // n0, n1: private hosts (private network)
  // n2: NAT router
  // n3: public server

  NodeContainer privateHosts; // n0, n1
  privateHosts.Create(2);

  Ptr<Node> natRouter = CreateObject<Node> (); // n2
  Ptr<Node> server = CreateObject<Node> ();    // n3

  NodeContainer privateLan;
  privateLan.Add (privateHosts.Get(0));
  privateLan.Add (privateHosts.Get(1));
  privateLan.Add (natRouter);

  NodeContainer publicLan;
  publicLan.Add (natRouter);
  publicLan.Add (server);

  // Create CSMA for each LAN segment
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer privateDevices = csma.Install(privateLan);
  NetDeviceContainer publicDevices = csma.Install(publicLan);

  InternetStackHelper internet;
  internet.Install (privateHosts);
  internet.Install (natRouter);
  internet.Install (server);

  // Assign IPs
  Ipv4AddressHelper ipPrivate, ipPublic;
  ipPrivate.SetBase ("10.0.0.0", "255.255.255.0");
  ipPublic.SetBase ("203.0.113.0", "255.255.255.0"); // RFC 5737 TEST-NET-3

  Ipv4InterfaceContainer privateIfs = ipPrivate.Assign(privateDevices);
  Ipv4InterfaceContainer publicIfs = ipPublic.Assign(publicDevices);

  // Set up NAT at natRouter between private (10.0.0.1) and public (203.0.113.1)
  Ipv4NATHelper natHelper;
  Ptr<Ipv4NAT> nat = natHelper.Install(natRouter);

  Ipv4Address natPrivateIface = privateIfs.Get(2).GetLocal(); // NAT's private IP
  Ipv4Address natPublicIface = publicIfs.Get(0).GetLocal();   // NAT's public IP
  
  // Configure NAT interfaces
  nat->SetAttribute ("InsideInterfaces", Ipv4InterfaceContainerValue (Ipv4InterfaceContainer(privateIfs.Get(2))));
  nat->SetAttribute ("OutsideInterfaces", Ipv4InterfaceContainerValue (Ipv4InterfaceContainer(publicIfs.Get(0))));

  // Enable NAT translation
  nat->AssignInside(privateIfs.Get(2).first);
  nat->AssignOutside(publicIfs.Get(0).first);

  // Set up routing
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> serverStaticRouting = ipv4RoutingHelper.GetStaticRouting(server->GetObject<Ipv4>());
  serverStaticRouting->SetDefaultRoute (publicIfs.Get(1).GetLocal (), 1);

  for (uint32_t i = 0; i < privateHosts.GetN (); ++i)
    {
      Ptr<Ipv4StaticRouting> hostStaticRouting = ipv4RoutingHelper.GetStaticRouting (
                                                   privateHosts.Get(i)->GetObject<Ipv4>());
      hostStaticRouting->SetDefaultRoute (privateIfs.Get(2).GetLocal (), 1);
    }

  // Install UDP server on the public server
  uint16_t serverPort = 4000;
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApp = udpServer.Install(server);
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Trace packet reception at server to view source address (NAT's public IP expected)
  Ptr<UdpServer> udpServerPtr = DynamicCast<UdpServer>(serverApp.Get(0));
  udpServerPtr->TraceConnectWithoutContext ("Rx", MakeCallback (&ServerRxTrace));

  // Install UDP clients on both private hosts
  UdpClientHelper udpClient (publicIfs.Get(1).GetLocal(), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (2));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (64));
  // Private hosts
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < privateHosts.GetN (); ++i)
    {
      ApplicationContainer clientApp = udpClient.Install(privateHosts.Get(i));
      clientApp.Start (Seconds (2.0 + i));
      clientApp.Stop (Seconds (8.0));
      // Optional: Trace the host's receive socket to log server response
      Ptr<UdpClient> udpClientPtr = DynamicCast<UdpClient>(clientApp.Get(0));
      Address sinkAddress (InetSocketAddress (privateIfs.Get(i).GetLocal(), udpClientPtr->GetLocalPort()));
      // No direct method to trace UdpClient receipt; we could use a raw socket trace if needed.
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}