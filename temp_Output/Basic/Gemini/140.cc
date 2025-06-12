#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

static void
RxCallback(Ptr<Socket> socket) {
  Address localAddress;
  socket->GetSockName(localAddress);
  NS_LOG_INFO("Socket " << socket << " received data from " << socket->GetRemote()
                         << " on " << localAddress);
  Ptr<Packet> packet;
  while ((packet = socket->Recv())) {
    NS_LOG_INFO("Received packet with size: " << packet->GetSize());
  }
}

static void
TxCallback(Ptr<Socket> socket, uint32_t txSpace) {
  NS_LOG_INFO("Socket " << socket << " Tx space: " << txSpace);
}

static void
CwndTracer(std::string context, uint32_t oldCwnd, uint32_t newCwnd) {
  NS_LOG_INFO(context << " CWND " << oldCwnd << " -> " << newCwnd);
}

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

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

  uint16_t port = 50000;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
  sinkSocket->Bind(sinkLocalAddress);
  sinkSocket->Listen();

  sinkSocket->SetRecvCallback(MakeCallback(&RxCallback));

  Ipv4Address serverAddress = interfaces.GetAddress(1);

  Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  sourceSocket->Bind();

  sourceSocket->SetConnectByInterface(0);
  sourceSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());

  sourceSocket->SetSendCallback(MakeCallback(&TxCallback));
  Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                               MakeCallback(&CwndTracer));

  Simulator::Schedule(Seconds(1.0), [&]() {
    Address remoteAddress(InetSocketAddress(serverAddress, port));
    sourceSocket->Connect(remoteAddress);
    uint32_t packetSize = 1024;
    Ptr<Packet> packet = Create<Packet>(packetSize);
    sourceSocket->Send(packet);
  });

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}