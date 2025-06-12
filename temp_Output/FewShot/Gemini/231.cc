#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

static void
RxCallback (Ptr<Socket> socket)
{
  Address remoteAddress;
  Ptr<Packet> packet = socket->RecvFrom (remoteAddress);
  std::cout << "Received one packet!" << std::endl;
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 5000;

  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), tid);
  InetSocketAddress iaddr (interfaces.GetAddress (1), port);
  recvSocket->Bind (iaddr);
  recvSocket->Listen ();
  recvSocket->SetRecvCallback (MakeCallback (&RxCallback));

  Ptr<Socket> sendSocket = Socket::CreateSocket (nodes.Get (0), tid);
  sendSocket->Connect (iaddr);

  Simulator::Schedule (Seconds (1.0), [&]() {
    Ptr<Packet> packet = Create<Packet> ((uint8_t *)"Hello world", 12);
    sendSocket->Send (packet);
  });

  Simulator::Stop (Seconds (2.0));

  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}