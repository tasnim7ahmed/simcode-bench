#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

void
RxCallback (Ptr<Socket> socket)
{
  Address localAddress;
  socket->GetSockName (localAddress);
  NS_LOG_INFO ("Socket " << socket << " received some data at " << Simulator::Now ());
}

void
TxCallback (Ptr<Socket> socket, uint32_t txSpace)
{
  NS_LOG_INFO ("Socket " << socket << " TxBuffer freed " << txSpace << " at time "
               << Simulator::Now ());
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

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

  uint16_t port = 50000;

  // Create a TCP socket to listen for connections
  Ptr<Socket> serverSocket = Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  serverSocket->Bind (local);
  serverSocket->Listen ();

  // Setup the client socket
  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  clientSocket->SetConnectTimeout (Seconds (1.0)); // added timeout to avoid blocking forever.
  InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (1), port);

  // Define a callback function to be called when a connection is established
  serverSocket->SetAcceptCallback (
      MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
      MakeCallback (&RxCallback));

  // Define a callback to be called when data is received.
  clientSocket->SetRecvCallback (MakeCallback (&RxCallback));
  clientSocket->SetSendCallback (MakeCallback (&TxCallback));

  // Connect the client to the server
  clientSocket->Connect (remote);

  // Schedule a function to send data from the client to the server
  Simulator::Schedule (Seconds (1.0), [clientSocket]() {
    Ptr<Packet> packet = Create<Packet> ((uint8_t *)"Hello World", 12);
    clientSocket->Send (packet);
  });

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}