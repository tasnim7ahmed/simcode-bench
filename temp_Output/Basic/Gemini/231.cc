#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include <iostream>

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

  uint16_t port = 9;

  Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
  recvSocket->Bind(local);
  recvSocket->SetRecvCallback(MakeCallback(&RxCallback));
  recvSocket->Listen();

  Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), port);
  sourceSocket->Connect(remote);

  Simulator::ScheduleWithContext(sourceSocket->GetNode()->GetId(), Seconds(1.0), [sourceSocket]() {
    Ptr<Packet> packet = Create<Packet>(100);
    sourceSocket->Send(packet);
  });

  Simulator::Stop(Seconds(2.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}