#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpMultiServerExample");

// Custom UDP server application
class CustomUdpServer : public Application
{
public:
  CustomUdpServer();
  virtual ~CustomUdpServer();

  void Setup(uint16_t port);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

CustomUdpServer::CustomUdpServer()
  : m_socket(0),
    m_port(0)
{
}

CustomUdpServer::~CustomUdpServer()
{
  m_socket = 0;
}

void CustomUdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void CustomUdpServer::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
}

void CustomUdpServer::StopApplication()
{
  if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
    }
}

void CustomUdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      uint32_t dataSize = packet->GetSize();
      InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Server (port " << m_port
        << ") received " << dataSize << " bytes from " << address.GetIpv4());
    }
}

// Custom UDP client application
class CustomUdpClient : public Application
{
public:
  CustomUdpClient();
  virtual ~CustomUdpClient();

  void Setup(const std::vector<Ipv4Address>& serverAddresses,
             const std::vector<uint16_t>& serverPorts,
             uint32_t pktSize,
             Time interval);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;
  void ScheduleTransmit();
  void SendPacket();

  std::vector<Ptr<Socket> > m_sockets;
  std::vector<Ipv4Address> m_serverAddresses;
  std::vector<uint16_t> m_serverPorts;
  uint32_t m_pktSize;
  Time m_interval;
  EventId m_sendEvent;
  uint32_t m_serverCount;
};

CustomUdpClient::CustomUdpClient()
  : m_pktSize(1024),
    m_interval(Seconds(1.0)),
    m_serverCount(0)
{
}

CustomUdpClient::~CustomUdpClient()
{
  m_sockets.clear();
}

void CustomUdpClient::Setup(const std::vector<Ipv4Address>& serverAddresses,
                            const std::vector<uint16_t>& serverPorts,
                            uint32_t pktSize,
                            Time interval)
{
  m_serverAddresses = serverAddresses;
  m_serverPorts = serverPorts;
  m_pktSize = pktSize;
  m_interval = interval;
  m_serverCount = serverAddresses.size();
}

void CustomUdpClient::StartApplication()
{
  m_sockets.clear();
  for (uint32_t i = 0; i < m_serverCount; ++i)
    {
      Ptr<Socket> sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      sock->Connect(InetSocketAddress(m_serverAddresses[i], m_serverPorts[i]));
      m_sockets.push_back(sock);
    }
  SendPacket();
}

void CustomUdpClient::StopApplication()
{
  if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }
  for (auto sock : m_sockets)
    {
      if (sock)
        {
          sock->Close();
        }
    }
}

void CustomUdpClient::ScheduleTransmit()
{
  m_sendEvent = Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
}

void CustomUdpClient::SendPacket()
{
  for (auto sock : m_sockets)
    {
      Ptr<Packet> p = Create<Packet>(m_pktSize);
      sock->Send(p);
    }
  ScheduleTransmit();
}

int main(int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable("UdpMultiServerExample", LOG_LEVEL_INFO);

  // 1 client + 2 servers = 3 nodes
  NodeContainer nodes;
  nodes.Create(3); // node 0: client, node 1: server1, node 2: server2

  // CSMA setup (the "switch" is internal to CSMA)
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP server on node 1 (Server 1)
  uint16_t server1Port = 9000;
  Ptr<CustomUdpServer> server1App = CreateObject<CustomUdpServer>();
  server1App->Setup(server1Port);
  nodes.Get(1)->AddApplication(server1App);
  server1App->SetStartTime(Seconds(0.0));
  server1App->SetStopTime(Seconds(10.0));

  // UDP server on node 2 (Server 2)
  uint16_t server2Port = 9001;
  Ptr<CustomUdpServer> server2App = CreateObject<CustomUdpServer>();
  server2App->Setup(server2Port);
  nodes.Get(2)->AddApplication(server2App);
  server2App->SetStartTime(Seconds(0.0));
  server2App->SetStopTime(Seconds(10.0));

  // UDP client on node 0
  std::vector<Ipv4Address> serverAddresses;
  std::vector<uint16_t> serverPorts;
  serverAddresses.push_back(interfaces.GetAddress(1)); // node 1
  serverPorts.push_back(server1Port);
  serverAddresses.push_back(interfaces.GetAddress(2)); // node 2
  serverPorts.push_back(server2Port);

  Ptr<CustomUdpClient> clientApp = CreateObject<CustomUdpClient>();
  clientApp->Setup(serverAddresses, serverPorts, 512, Seconds(1.0));
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0)); // start after servers
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}