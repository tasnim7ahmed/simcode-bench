#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpRawSocket");

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

  Address m_peerAddress;
  Ptr<Socket> m_socket;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient()
    : m_socket(nullptr),
      m_packetSize(0),
      m_numPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

UdpClient::~UdpClient()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void UdpClient::Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void UdpClient::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
  m_socket->Connect(m_peerAddress);
  m_socket->SetAllowBroadcast(true);

  SendPacket();
}

void UdpClient::StopApplication(void)
{
  m_running = false;
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

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
    Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
    m_sendEvent = Simulator::Schedule(tNext, &UdpClient::SendPacket, this);
  }
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

  void ReceivePacket(Ptr<Socket> socket);
  void HandleRead(Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_running;
};

UdpServer::UdpServer()
    : m_port(0),
      m_socket(nullptr),
      m_running(false)
{
}

UdpServer::~UdpServer()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void UdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void UdpServer::StartApplication(void)
{
  m_running = true;

  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpServer::ReceivePacket, this));
}

void UdpServer::StopApplication(void)
{
  m_running = false;
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void UdpServer::ReceivePacket(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);
  if (packet != nullptr)
  {
    std::cout << Simulator::Now().As(Seconds) << " Server received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort() << std::endl;
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("CustomUdpRawSocket", LOG_LEVEL_INFO);

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
  ApplicationContainer serverApp = server.Install(nodes.Get(1));

  Ptr<UdpServer> customServer = DynamicCast<UdpServer>(serverApp.Get(0));
  customServer->Setup(port);
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));

  Ptr<UdpClient> customClient = DynamicCast<UdpClient>(client.GetClient());
  customClient->Setup(InetSocketAddress(interfaces.GetAddress(1), port), 1024, 100, DataRate("1Mbps"));
  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}