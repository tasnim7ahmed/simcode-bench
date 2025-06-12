#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/log.h"

#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpSocket");

class UdpClient : public Application
{
public:
  UdpClient();
  virtual ~UdpClient();

  void Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);
  void ScheduleTransmit(void);

  Address m_remoteAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Ptr<Socket> m_socket;
  Time m_interPacketInterval;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient() : m_packetSize(0),
                           m_numPackets(0),
                           m_dataRate(0),
                           m_socket(nullptr),
                           m_interPacketInterval(Seconds(0)),
                           m_sendEvent(),
                           m_packetsSent(0)
{
}

UdpClient::~UdpClient()
{
  m_socket = nullptr;
}

void UdpClient::Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_remoteAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = Time(Seconds(8 * (double)m_packetSize / (double)m_dataRate.GetBitRate()));
}

void UdpClient::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket->Bind() == -1)
  {
    NS_FATAL_ERROR("Failed to bind socket");
  }
  m_socket->Connect(m_remoteAddress);
  m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>, Ptr<Packet>, const Address &>());
  m_packetsSent = 0;
  ScheduleTransmit();
}

void UdpClient::StopApplication(void)
{
  Simulator::Cancel(m_sendEvent);
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void UdpClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_numPackets)
  {
    ScheduleTransmit();
  }
}

void UdpClient::ScheduleTransmit(void)
{
  m_sendEvent = Simulator::Schedule(m_interPacketInterval, &UdpClient::SendPacket, this);
}

class UdpServer : public Application
{
public:
  UdpServer();
  virtual ~UdpServer();

  void Setup(uint16_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);
  void LogPacket(Ptr<Packet> packet, const Address &from);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  Address m_localAddress;
};

UdpServer::UdpServer() : m_port(0),
                           m_socket(nullptr)
{
}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

void UdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void UdpServer::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  if (m_socket->Bind(local) == -1)
  {
    NS_FATAL_ERROR("Failed to bind socket");
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
  m_socket->SetAllowBroadcast(true);
}

void UdpServer::StopApplication(void)
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void UdpServer::HandleRead(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
  {
    LogPacket(packet, from);
  }
}

void UdpServer::LogPacket(Ptr<Packet> packet, const Address &from)
{
  InetSocketAddress iaddr = InetSocketAddress::ConvertFrom(from);
  NS_LOG_UNCOND("Received " << packet->GetSize() << " bytes from " << iaddr.GetIpv4() << " port " << iaddr.GetPort());
}

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("CustomUdpSocket", LOG_LEVEL_INFO);

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
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));

  Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApps.Get(0));
  udpServer->Setup(port);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  DataRate dataRate("1Mbps");

  UdpClientHelper client(interfaces.GetAddress(1), port);
  ApplicationContainer clientApps = client.Install(nodes.Get(0));

  Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(clientApps.Get(0));
  udpClient->Setup(InetSocketAddress(interfaces.GetAddress(1), port), packetSize, numPackets, dataRate);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}