#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
ReceivePacket(Ptr<Socket> socket)
{
  while (Ptr<Packet> pkt = socket->Recv())
    {
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Server received "
                                                  << pkt->GetSize() << " bytes");
    }
}

void
SendPacket(Ptr<Socket> socket, Ipv4Address dstaddr, uint16_t port, uint32_t pktSize, uint32_t pktCount, Time interPacketInterval)
{
  if (pktCount > 0)
    {
      Ptr<Packet> packet = Create<Packet>(pktSize);
      socket->SendTo(packet, 0, InetSocketAddress(dstaddr, port));
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s Client sent "
                                                  << pktSize << " bytes to "
                                                  << dstaddr);
      Simulator::Schedule(interPacketInterval, &SendPacket, socket, dstaddr, port, pktSize, pktCount - 1, interPacketInterval);
    }
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("TcpSocketBase", LOG_LEVEL_WARN);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;
  uint32_t pktSize = 1024;
  uint32_t pktCount = 10;
  Time interPacketInterval = MilliSeconds(200);

  // Server socket
  Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
  InetSocketAddress serverAddress = InetSocketAddress(Ipv4Address::GetAny(), port);
  recvSocket->Bind(serverAddress);
  recvSocket->Listen();
  recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

  // Client socket
  Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  sendSocket->Connect(InetSocketAddress(interfaces.GetAddress(1), port));

  Simulator::Schedule(Seconds(1.0), &SendPacket, sendSocket, interfaces.GetAddress(1), port, pktSize, pktCount, interPacketInterval);

  Simulator::Stop(Seconds(5.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}