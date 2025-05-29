#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpRelay");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet != nullptr)
    {
      std::cout << Simulator::Now ().Seconds () << " Received one packet!" << std::endl;
    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

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

  uint16_t port = 9;

  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces2.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (2), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}