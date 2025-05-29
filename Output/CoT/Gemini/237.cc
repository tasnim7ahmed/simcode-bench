#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaRelayExample");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);

  if (packet->GetSize () > 0)
    {
      NS_LOG_INFO ("Received one packet!");
    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("CsmaRelayExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (2)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2)); // Server on node 2
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (interfaces.GetAddress (2), port); // Client sends to server
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0)); // Client on node 0
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  ns3UdpSocket->Bind (local);
  ns3UdpSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}