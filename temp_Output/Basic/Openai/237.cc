#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaThreeNodeRelay");

int
main (int argc, char *argv[])
{
  // Command line arguments (optional) for custom settings
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create three nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Set up csma channel
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Install CSMA devices
  NetDeviceContainer devices = csma.Install (nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Node mapping:
  // nodes.Get(0): UDP Client
  // nodes.Get(1): Relay
  // nodes.Get(2): UDP Server

  uint16_t serverPort = 4000;

  // Install UDP server on node 2 (last node)
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApps = server.Install (nodes.Get(2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP client on node 0 (first node), dest IP = server, via relay
  // Because single CSMA subnet, routing isn't strictly necessary, but we set up apps as if they are at ends
  UdpClientHelper client (interfaces.GetAddress (2), serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (nodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable packet forwarding on relay node (node 1)
  Ptr<Ipv4> relayIpv4 = nodes.Get(1)->GetObject<Ipv4> ();
  relayIpv4->SetAttribute ("IpForward", BooleanValue (true));

  // Enable routing if needed in general (not strictly necessary on a single LAN)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Enable server logging of received packets
  Config::ConnectWithoutContext ("/NodeList/2/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback ([] (Ptr<const Packet> packet, const Address &address) {
    NS_LOG_UNCOND ("Server received " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom (address).GetIpv4 ());
  }));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}