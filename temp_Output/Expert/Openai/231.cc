#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
RxCallback(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_UNCOND("Server received " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << "s");
    }
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 5000;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  // Server socket
  Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
  InetSocketAddress localAddress = InetSocketAddress(Ipv4Address::GetAny(), port);
  serverSocket->Bind(localAddress);
  serverSocket->Listen();
  serverSocket->SetRecvCallback(MakeCallback(&RxCallback));

  // Client socket
  Simulator::Schedule(Seconds(0.1), [&]{
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    clientSocket->Connect(serverAddress);
    Ptr<Packet> packet = Create<Packet>(512);
    clientSocket->Send(packet);
    clientSocket->Close();
  });

  Simulator::Stop(Seconds(0.3));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}