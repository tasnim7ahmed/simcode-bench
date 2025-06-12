#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
ReceivePacket (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      std::cout << "At " << Simulator::Now ().GetSeconds ()
                << "s, server received " << packet->GetSize () << " bytes" << std::endl;
    }
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create and bind the server socket
  uint16_t port = 8080;
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->Listen ();
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  // Schedule the client to connect, send small data (e.g., 512 bytes), then close
  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (1), port);

  Simulator::Schedule (Seconds (0.2), &Socket::Connect, clientSocket, remote);

  Ptr<Packet> pkt = Create<Packet> (512);
  Simulator::Schedule (Seconds (0.3), &Socket::Send, clientSocket, pkt);

  Simulator::Schedule (Seconds (0.4), &Socket::Close, clientSocket);

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}