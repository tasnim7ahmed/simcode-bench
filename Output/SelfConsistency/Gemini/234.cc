#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RelayExample");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet->GetSize () > 0)
    {
      std::cout << Simulator::Now ().AsTimeStep () << " Received one packet with size " << packet->GetSize () << " from " << from << std::endl;
    }
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("RelayExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // Client, Relay, Server

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Server setup
  uint16_t port = 9;
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  // Client setup
  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (interfaces2.GetAddress (1), port);
  clientSocket->Connect (remote);


  // Relay setup (forwarding)
  Ptr<Node> relayNode = nodes.Get (1);
  Ptr<Ipv4> relayIpv4 = relayNode->GetObject<Ipv4> ();
  relayIpv4->SetForwarding (0, true);  // Enable IPv4 forwarding on the relay


  // Create an OnOff application to send UDP packets from client to server via relay
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces2.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("100kbps"));
  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}