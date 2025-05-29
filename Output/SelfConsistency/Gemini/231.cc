#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServer");

static void
RxCallback(Ptr<Socket> socket)
{
  Address remoteAddress;
  socket->RecvFrom(remoteAddress);
  NS_LOG_INFO("Received one packet!");
}

int
main(int argc, char* argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("TcpClientServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9; // well-known echo port number

  // Create a server socket to listen for connections
  Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
  serverSocket->Bind(local);
  serverSocket->Listen();
  serverSocket->SetRecvCallback(MakeCallback(&RxCallback));

  // Create a client socket and connect to the server
  Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), port);
  clientSocket->Connect(remote);

  // Schedule a packet to be sent from client to server
  Simulator::ScheduleWithContext(
      nodes.Get(0)->GetId(), Seconds(1.0), MakeCallback(&Socket::Send, clientSocket), Create<Packet>(100));

  Simulator::Stop(Seconds(2.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}