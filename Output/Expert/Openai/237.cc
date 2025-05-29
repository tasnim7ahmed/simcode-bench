#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
ReceivePacketCallback (Ptr<OutputStreamWrapper> stream,
                      Ptr<const Packet> packet,
                      const Address &address)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " Received "
                       << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom (address).GetIpv4 () << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t serverPort = 4000;
  uint32_t maxPacketCount = 10;
  Time interPacketInterval = Seconds (1.0);
  uint32_t packetSize = 512;

  // UDP server on node 2
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApp = server.Install (nodes.Get (2));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (15.0));

  // UDP client on node 0, sends to node 2 (must route via relay node 1)
  UdpClientHelper client (interfaces.GetAddress (2), serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (15.0));

  // Enable routing on relay node (node 1)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Logging received packets at the server
  Ptr<UdpServer> serverPtr = DynamicCast<UdpServer> (serverApp.Get (0));
  Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> (&std::cout);
  serverPtr->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&ReceivePacketCallback, stream));

  Simulator::Stop (Seconds (16.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}