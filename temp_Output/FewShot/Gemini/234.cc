#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"

using namespace ns3;

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  socket->SendTo (packet, 0, from);
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

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

  uint16_t port = 9;

  // Server Setup (Node 2)
  Ptr<Socket> serverSocket = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  InetSocketAddress serverAddr (interfaces2.GetAddress (1), port);
  serverSocket->Bind (serverAddr);
  serverSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

    //Relay setup (Node 1)
  Ptr<Socket> relayRxSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  InetSocketAddress relayRxAddr (interfaces1.GetAddress (1), port);
  relayRxSocket->Bind (relayRxAddr);

  Ptr<Socket> relayTxSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  InetSocketAddress relayTxAddr (interfaces2.GetAddress (1), port);


  relayRxSocket->SetRecvCallback ([relayTxSocket, relayTxAddr] (Ptr<Socket> socket) {
        Address from;
        Ptr<Packet> packet = socket->RecvFrom (from);
        relayTxSocket->SendTo (packet, 0, relayTxAddr);
  });


  // Client Setup (Node 0)
  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress serverAddress (interfaces1.GetAddress (1), port);
  clientSocket->Connect (serverAddress);

  Simulator::Schedule (Seconds (1.0), [clientSocket, serverAddress] () {
        Ptr<Packet> packet = Create<Packet> ((uint8_t*) "Hello, Relay!", 13);
        clientSocket->Send (packet);
  });

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}