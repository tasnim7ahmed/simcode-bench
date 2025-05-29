#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpSocketExample");

static void
RxCallback (Ptr<Socket> socket)
{
  Address remoteAddress;
  socket->RecvFrom (remoteAddress);
  NS_LOG_INFO ("Received one packet!");
}

static void
TxCallback (Ptr<Socket> socket, uint32_t txSpace)
{
  NS_LOG_INFO ("txSpace: " << txSpace);
  socket->SetSendBuffer (txSpace);
}

static void
CwndTracer (std::string prefix, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_INFO (prefix << "Cwnd " << oldCwnd << "->" << newCwnd);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("TcpSocketExample", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

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

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  ns3TcpSocket->Bind (local);

  ns3TcpSocket->SetRecvCallback (MakeCallback (&RxCallback));

  Ipv4Address serverAddress = interfaces.GetAddress (1);
  InetSocketAddress remote = InetSocketAddress (serverAddress, port);
  ns3TcpSocket->Connect (remote);

  ns3TcpSocket->SetSendCallback (MakeCallback (&TxCallback));

  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());
  clientSocket->Bind ();
  clientSocket->Connect (InetSocketAddress (interfaces.GetAddress (0), port));

  clientSocket->SetRecvCallback (MakeCallback (&RxCallback));

  Simulator::ScheduleWithContext (clientSocket->GetNode ()->GetId (), Seconds (1.0), [&](){
    Ptr<Packet> packet = Create<Packet> (1024);
    clientSocket->Send (packet);
  });

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-socket-example.tr"));

  Simulator::Stop (Seconds (5.0));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}