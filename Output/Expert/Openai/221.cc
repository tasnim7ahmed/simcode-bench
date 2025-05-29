#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerApp : public Application
{
public:
  UdpServerApp() {}
  virtual ~UdpServerApp() {}
  void Setup(uint16_t port);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

void UdpServerApp::Setup(uint16_t port)
{
  m_port = port;
}

void UdpServerApp::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
}

void UdpServerApp::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void UdpServerApp::HandleRead(Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom(from))
  {
    InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
    NS_LOG_UNCOND("Server received " << packet->GetSize() << " bytes from " << address.GetIpv4() << ":" << address.GetPort() << " at " << Simulator::Now().GetSeconds() << "s");
  }
}

class UdpClientApp : public Application
{
public:
  UdpClientApp() {}
  virtual ~UdpClientApp() {}
  void Setup(std::vector<Ipv4Address> serverAddresses, std::vector<uint16_t> serverPorts, uint32_t packetSize, uint32_t nPackets, Time interval);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;
  void ScheduleTx();
  void SendPacket();

  std::vector<Ipv4Address> m_serverAddresses;
  std::vector<uint16_t> m_serverPorts;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
};

void UdpClientApp::Setup(std::vector<Ipv4Address> serverAddresses, std::vector<uint16_t> serverPorts, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_serverAddresses = serverAddresses;
  m_serverPorts = serverPorts;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
  m_sent = 0;
}

void UdpClientApp::StartApplication()
{
  m_sent = 0;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  ScheduleTx();
}

void UdpClientApp::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
  Simulator::Cancel(m_sendEvent);
}

void UdpClientApp::ScheduleTx()
{
  if (m_sent < m_nPackets)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &UdpClientApp::SendPacket, this);
  }
}

void UdpClientApp::SendPacket()
{
  for (size_t i = 0; i < m_serverAddresses.size(); ++i)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    Address dest = InetSocketAddress(m_serverAddresses[i], m_serverPorts[i]);
    m_socket->SendTo(packet, 0, dest);
    NS_LOG_UNCOND("Client sent " << m_packetSize << " bytes to " << m_serverAddresses[i] << ":" << m_serverPorts[i] << " at " << Simulator::Now().GetSeconds() << "s");
  }
  ++m_sent;
  ScheduleTx();
}

int main(int argc, char *argv[])
{
  LogComponentEnable("UdpServerApp", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClientApp", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(3); // node 0: client, node 1: server1, node 2: server2

  NodeContainer csmaNodes;
  csmaNodes.Add(nodes.Get(0));
  csmaNodes.Add(nodes.Get(1));
  csmaNodes.Add(nodes.Get(2));

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(csmaNodes);

  InternetStackHelper stack;
  stack.Install(csmaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Application parameters
  uint16_t server1Port = 9001;
  uint16_t server2Port = 9002;
  uint32_t packetSize = 1024;
  double intervalSec = 1.0;
  uint32_t nPackets = 10;
  Time interPacketInterval = Seconds(intervalSec);

  // Servers
  Ptr<UdpServerApp> server1 = CreateObject<UdpServerApp>();
  server1->Setup(server1Port);
  nodes.Get(1)->AddApplication(server1);
  server1->SetStartTime(Seconds(0.0));
  server1->SetStopTime(Seconds(10.0));

  Ptr<UdpServerApp> server2 = CreateObject<UdpServerApp>();
  server2->Setup(server2Port);
  nodes.Get(2)->AddApplication(server2);
  server2->SetStartTime(Seconds(0.0));
  server2->SetStopTime(Seconds(10.0));

  // Client
  Ptr<UdpClientApp> client = CreateObject<UdpClientApp>();
  std::vector<Ipv4Address> serverAddresses = {interfaces.GetAddress(1), interfaces.GetAddress(2)};
  std::vector<uint16_t> serverPorts = {server1Port, server2Port};
  client->Setup(serverAddresses, serverPorts, packetSize, nPackets, interPacketInterval);
  nodes.Get(0)->AddApplication(client);
  client->SetStartTime(Seconds(1.0));
  client->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}