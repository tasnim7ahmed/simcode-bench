#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

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

  uint16_t port = 8080;
  Address sinkLocalAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
  ns3TcpSocket->SetConnectTimeout(Seconds(10.0));

  TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
  Ptr<GenericClient> client = CreateObject<GenericClient>();
  client->SetTypeId(tid);
  client->SetRemoteAddress(interfaces.GetAddress(1));
  client->SetRemotePort(port);
  client->SetSocket(ns3TcpSocket);
  client->SetPacketSize(1024);
  client->SetNumPackets(10);
  client->SetPacketInterval(Seconds(1.0));

  nodes.Get(0)->AddApplication(client);
  client->SetStartTime(Seconds(2.0));
  client->SetStopTime(Seconds(10.0));
  client->StartApplication();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

class GenericClient : public Application
{
public:
  GenericClient();
  virtual ~GenericClient();

  void SetRemoteAddress(Ipv4Address addr);
  void SetRemotePort(uint16_t port);
  void SetSocket(Ptr<Socket> socket);
  void SetPacketSize(uint32_t packetSize);
  void SetNumPackets(uint32_t numPackets);
  void SetPacketInterval(Time packetInterval);
  void SetTypeId (TypeId tid);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleTransmit(void);
  void SendPacket(void);
  void SocketConnect(void);
  void SocketConnected(Ptr<Socket> socket);
  void SocketError(Ptr<Socket> socket);
  void SocketDataSent(Ptr<Socket> socket, uint32_t bytesSent);

  TypeId m_tid;
  Ipv4Address m_remoteAddress;
  uint16_t m_remotePort;
  Ptr<Socket> m_socket;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  Time m_packetInterval;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
  bool m_connected;
};

GenericClient::GenericClient() :
  m_remoteAddress(Ipv4Address::GetZero()),
  m_remotePort(0),
  m_socket(nullptr),
  m_packetSize(0),
  m_numPackets(0),
  m_packetInterval(Seconds(0)),
  m_packetsSent(0),
  m_sendEvent(),
  m_connected(false)
{
}

GenericClient::~GenericClient()
{
  m_socket = nullptr;
}

void
GenericClient::SetRemoteAddress(Ipv4Address addr)
{
  m_remoteAddress = addr;
}

void
GenericClient::SetRemotePort(uint16_t port)
{
  m_remotePort = port;
}

void
GenericClient::SetSocket(Ptr<Socket> socket)
{
  m_socket = socket;
}

void
GenericClient::SetPacketSize(uint32_t packetSize)
{
  m_packetSize = packetSize;
}

void
GenericClient::SetNumPackets(uint32_t numPackets)
{
  m_numPackets = numPackets;
}

void
GenericClient::SetPacketInterval(Time packetInterval)
{
  m_packetInterval = packetInterval;
}

void
GenericClient::SetTypeId (TypeId tid)
{
  m_tid = tid;
}

void
GenericClient::StartApplication(void)
{
  m_packetsSent = 0;
  m_socket->Bind();
  m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>, Ptr<Packet>, const Address &>());
  m_socket->SetConnectCallback(MakeCallback(&GenericClient::SocketConnected, this),
                                  MakeCallback(&GenericClient::SocketError, this));
  m_socket->SetSendCallback(MakeCallback(&GenericClient::SocketDataSent, this));
  SocketConnect();
}

void
GenericClient::StopApplication(void)
{
  m_connected = false;
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
}

void
GenericClient::ScheduleTransmit(void)
{
  if (m_packetsSent < m_numPackets && m_connected)
  {
    m_sendEvent = Simulator::Schedule(m_packetInterval, &GenericClient::SendPacket, this);
  }
}

void
GenericClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  m_packetsSent++;
  ScheduleTransmit();
}

void
GenericClient::SocketConnect(void)
{
  m_socket->Connect(InetSocketAddress(m_remoteAddress, m_remotePort));
}

void
GenericClient::SocketConnected(Ptr<Socket> socket)
{
  m_connected = true;
  ScheduleTransmit();
}

void
GenericClient::SocketError(Ptr<Socket> socket)
{
  NS_LOG_INFO("Socket error!");
}

void
GenericClient::SocketDataSent(Ptr<Socket> socket, uint32_t bytesSent)
{
  NS_LOG_INFO("Data sent: " << bytesSent);
}