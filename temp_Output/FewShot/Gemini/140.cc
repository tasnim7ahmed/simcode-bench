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

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

static void
RxCallback(Ptr<Socket> socket)
{
  Address remoteAddress;
  Ptr<Packet> packet = socket->RecvFrom(remoteAddress);
  NS_LOG_INFO("Received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(remoteAddress).GetIpv4() << ":" << InetSocketAddress::ConvertFrom(remoteAddress).GetPort());
}

static void
CwndChange(Ptr<Socket> socket, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_INFO("Congestion window changed from " << oldCwnd << " to " << newCwnd);
}

static void
RttEstimate(Ptr<Socket> socket, Time rtt)
{
  NS_LOG_INFO("Round trip time estimate: " << rtt);
}

int
main(int argc, char* argv[])
{
  LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress remoteAddress(interfaces.GetAddress(1), 8080);
  ns3TcpSocket->Connect(remoteAddress);

  ns3TcpSocket->SetRecvCallback(MakeCallback(&RxCallback));
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange));
  ns3TcpSocket->TraceConnectWithoutContext("RTT", MakeCallback(&RttEstimate));

  Ptr<Socket> acceptingSocket = Socket::CreateSocket(nodes.Get(1), tid);
  InetSocketAddress localAddress(Ipv4Address::GetAny(), 8080);
  acceptingSocket->Bind(localAddress);
  acceptingSocket->Listen();

  acceptingSocket->SetAcceptCallback(
    MakeCallback(&Socket::DoAccept),
    MakeCallback(&Socket::DoDispose));

  Simulator::Schedule(Seconds(1.0), [&]() {
    Ptr<Packet> packet = Create<Packet>((uint8_t*) "Hello World", 11);
    ns3TcpSocket->Send(packet);
  });

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}