#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/ipv4-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpMultiServerExample");

class CustomUdpServer : public Application
{
public:
  CustomUdpServer();
  virtual ~CustomUdpServer();

  void Setup(uint16_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  uint16_t        m_port;
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

void
CustomUdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void
CustomUdpServer::StartApplication(void)
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
    }
}

void
CustomUdpServer::StopApplication(void)
{
  if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
    }
}

void
CustomUdpServer::HandleRead(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
    {
      if (packet->GetSize() > 0)
        {
          InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
          NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Server (port " << m_port <<
            ") received " << packet->GetSize() << " bytes from " << address.GetIpv4());
        }
    }
}

class CustomUdpClient : public Application
{
public:
  CustomUdpClient();
  virtual ~CustomUdpClient();

  void Setup(std::vector<Ipv4Address> serverAddresses,
             std::vector<uint16_t> serverPorts,
             uint32_t packetSize,
             Time interval);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void ScheduleTx(void);
  void SendPacket(void);

  std::vector<Ptr<Socket> > m_sockets;
  std::vector<Ipv4Address>  m_serverAddresses;
  std::vector<uint16_t>     m_serverPorts;
  uint32_t                  m_packetSize;
  EventId                   m_sendEvent;
  Time                      m_interval;
};

CustomUdpClient::CustomUdpClient()
  : m_packetSize(0)
{
}

CustomUdpClient::~CustomUdpClient()
{
  for (auto s : m_sockets)
    {
      s = 0;
    }
}

void
CustomUdpClient::Setup(std::vector<Ipv4Address> serverAddresses,
                       std::vector<uint16_t> serverPorts,
                       uint32_t packetSize,
                       Time interval)
{
  m_serverAddresses = serverAddresses;
  m_serverPorts = serverPorts;
  m_packetSize = packetSize;
  m_interval = interval;
}

void
CustomUdpClient::StartApplication(void)
{
  m_sockets.clear();
  for (size_t i = 0; i < m_serverAddresses.size(); ++i)
    {
      Ptr<Socket> socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_sockets.push_back(socket);
    }
  ScheduleTx();
}

void
CustomUdpClient::StopApplication(void)
{
  Simulator::Cancel(m_sendEvent);
  for (auto s : m_sockets)
    {
      if (s)
        s->Close();
    }
}

void
CustomUdpClient::SendPacket(void)
{
  for (size_t i = 0; i < m_sockets.size(); ++i)
    {
      Ptr<Packet> p = Create<Packet>(m_packetSize);
      m_sockets[i]->SendTo(p, 0, InetSocketAddress(m_serverAddresses[i], m_serverPorts[i]));
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Client sent " << m_packetSize
        << " bytes to " << m_serverAddresses[i] << ":" << m_serverPorts[i]);
    }
  ScheduleTx();
}

void
CustomUdpClient::ScheduleTx(void)
{
  m_sendEvent = Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
}


int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpMultiServerExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3); // Node 0: client, Node 1: server1, Node 2: server2

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Set up servers
  uint16_t port1 = 8000;
  uint16_t port2 = 8001;

  Ptr<CustomUdpServer> serverApp1 = CreateObject<CustomUdpServer>();
  serverApp1->Setup(port1);
  nodes.Get(1)->AddApplication(serverApp1);
  serverApp1->SetStartTime(Seconds(0.0));
  serverApp1->SetStopTime(Seconds(10.0));

  Ptr<CustomUdpServer> serverApp2 = CreateObject<CustomUdpServer>();
  serverApp2->Setup(port2);
  nodes.Get(2)->AddApplication(serverApp2);
  serverApp2->SetStartTime(Seconds(0.0));
  serverApp2->SetStopTime(Seconds(10.0));

  // Set up client
  std::vector<Ipv4Address> serverAddresses = {
      interfaces.GetAddress(1), // server1
      interfaces.GetAddress(2)  // server2
  };
  std::vector<uint16_t> serverPorts = {port1, port2};

  Ptr<CustomUdpClient> clientApp = CreateObject<CustomUdpClient>();
  clientApp->Setup(serverAddresses, serverPorts, 512, Seconds(1.0));
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}