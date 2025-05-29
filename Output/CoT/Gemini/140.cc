#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"

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
CwndTracer (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().AsDouble () << " " << newCwnd);
}

int main (int argc, char *argv[])
{
  LogComponent::Enable ("TcpSocketExample", LOG_LEVEL_INFO);

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

  uint16_t port = 9;

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  ns3TcpSocket->Bind (local);
  ns3TcpSocket->Listen ();
  ns3TcpSocket->SetRecvCallback (MakeCallback (&RxCallback));

  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (1), port);
  clientSocket->Connect (remote);

  clientSocket->SetAllowBroadcast (true);

  Simulator::Schedule (Seconds (1.0), [&]{
    Ptr<Packet> packet = Create<Packet> (1024);
    clientSocket->Send (packet);
  });

  Simulator::Schedule (Seconds (2.0), [&]{
    Ptr<Packet> packet = Create<Packet> (1024);
    clientSocket->Send (packet);
  });

  Simulator::Schedule (Seconds (3.0), [&]{
    Ptr<Packet> packet = Create<Packet> (1024);
    clientSocket->Send (packet);
  });

  Simulator::Stop (Seconds (5.0));

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-socket-example.tr"));
  pointToPoint.EnablePcapAll ("tcp-socket-example");

  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));

  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}