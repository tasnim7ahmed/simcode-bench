#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiUdpServerClientSimulation");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId();
  UdpServer();
  virtual ~UdpServer();

protected:
  void DoDispose() override;

private:
  void StartApplication() override;
  void StopApplication() override;
  void HandleRead(Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
};

TypeId
UdpServer::GetTypeId()
{
  static TypeId tid = TypeId("UdpServer")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<UdpServer>()
                          .AddAttribute("Port",
                                        "Listening port number",
                                        UintegerValue(9),
                                        MakeUintegerAccessor(&UdpServer::m_port),
                                        MakeUintegerChecker<uint16_t>());
  return tid;
}

UdpServer::UdpServer()
    : m_port(0), m_socket(nullptr)
{
  NS_LOG_FUNCTION(this);
}

UdpServer::~UdpServer()
{
  NS_LOG_FUNCTION(this);
}

void
UdpServer::DoDispose()
{
  NS_LOG_FUNCTION(this);
  m_socket = nullptr;
  Application::DoDispose();
}

void
UdpServer::StartApplication()
{
  NS_LOG_FUNCTION(this);
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    if (m_socket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void
UdpServer::StopApplication()
{
  NS_LOG_FUNCTION(this);
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
  }
}

class UdpClient : public Application
{
public:
  static TypeId GetTypeId();
  UdpClient();
  virtual ~UdpClient();

protected:
  void DoDispose() override;

private:
  void StartApplication() override;
  void StopApplication() override;
  void SendPacket();

  Ipv4Address m_server1Ip;
  Ipv4Address m_server2Ip;
  uint16_t m_server1Port;
  uint16_t m_server2Port;
  uint32_t m_packetSize;
  Time m_interval;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  bool m_running;
};

TypeId
UdpClient::GetTypeId()
{
  static TypeId tid = TypeId("UdpClient")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<UdpClient>()
                          .AddAttribute("Server1Ip", "IP address of server 1",
                                        Ipv4AddressValue(),
                                        MakeIpv4Accessor(&UdpClient::m_server1Ip),
                                        MakeIpv4Checker())
                          .AddAttribute("Server2Ip", "IP address of server 2",
                                        Ipv4AddressValue(),
                                        MakeIpv4Accessor(&UdpClient::m_server2Ip),
                                        MakeIpv4Checker())
                          .AddAttribute("Server1Port", "Port of server 1",
                                        UintegerValue(9),
                                        MakeUintegerAccessor(&UdpClient::m_server1Port),
                                        MakeUintegerChecker<uint16_t>())
                          .AddAttribute("Server2Port", "Port of server 2",
                                        UintegerValue(10),
                                        MakeUintegerAccessor(&UdpClient::m_server2Port),
                                        MakeUintegerChecker<uint16_t>())
                          .AddAttribute("PacketSize", "Size of packets to send",
                                        UintegerValue(512),
                                        MakeUintegerAccessor(&UdpClient::m_packetSize),
                                        MakeUintegerChecker<uint32_t>())
                          .AddAttribute("Interval", "Time between packets",
                                        TimeValue(Seconds(1.0)),
                                        MakeTimeAccessor(&UdpClient::m_interval),
                                        MakeTimeChecker());
  return tid;
}

UdpClient::UdpClient()
    : m_server1Port(0), m_server2Port(0), m_packetSize(512), m_interval(Seconds(1.0)),
      m_socket(nullptr), m_running(false)
{
  NS_LOG_FUNCTION(this);
}

UdpClient::~UdpClient()
{
  NS_LOG_FUNCTION(this);
}

void
UdpClient::DoDispose()
{
  NS_LOG_FUNCTION(this);
  m_socket = nullptr;
  Application::DoDispose();
}

void
UdpClient::StartApplication()
{
  NS_LOG_FUNCTION(this);
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
  }

  m_running = true;
  SendPacket();
}

void
UdpClient::StopApplication()
{
  NS_LOG_FUNCTION(this);
  m_running = false;
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void
UdpClient::SendPacket()
{
  NS_LOG_FUNCTION(this);
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  InetSocketAddress dest1 = InetSocketAddress(m_server1Ip, m_server1Port);
  InetSocketAddress dest2 = InetSocketAddress(m_server2Ip, m_server2Port);

  m_socket->SendTo(packet, 0, dest1);
  m_socket->SendTo(packet, 0, dest2);

  NS_LOG_INFO("Sent packets to servers at " << dest1 << " and " << dest2);

  m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("MultiUdpServerClientSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3); // node 0: client, node 1: server1, node 2: server2

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(10000000)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install server applications
  UdpServerHelper server1(9); // Server 1 on port 9
  ApplicationContainer apps1 = server1.Install(nodes.Get(1));
  apps1.Start(Seconds(0.0));
  apps1.Stop(Seconds(10.0));

  UdpServerHelper server2(10); // Server 2 on port 10
  ApplicationContainer apps2 = server2.Install(nodes.Get(2));
  apps2.Start(Seconds(0.0));
  apps2.Stop(Seconds(10.0));

  // Install client application
  UdpClientHelper client;
  client.SetAttribute("Server1Ip", Ipv4AddressValue(interfaces.GetAddress(1)));
  client.SetAttribute("Server2Ip", Ipv4AddressValue(interfaces.GetAddress(2)));
  client.SetAttribute("Server1Port", UintegerValue(9));
  client.SetAttribute("Server2Port", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}