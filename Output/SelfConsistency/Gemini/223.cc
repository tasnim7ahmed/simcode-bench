#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiUdp");

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
  void SocketReceive(Ptr<Socket> socket);
  void ChangeRate(DataRate newRate);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Ptr<Socket> m_socket;
  Address m_localAddress;
  uint32_t m_packetsSent;
  bool m_running;
  Time m_interPacketInterval;
  EventId m_sendEvent;
};

UdpClient::UdpClient()
    : m_packetSize(0),
      m_numPackets(0),
      m_dataRate(0),
      m_socket(nullptr),
      m_localAddress(),
      m_packetsSent(0),
      m_running(false),
      m_interPacketInterval()
{
}

UdpClient::~UdpClient()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
  }
}

void
UdpClient::Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = Time(Seconds(8 * (double)packetSize / dataRate.GetBitRate()));
}

void
UdpClient::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));

  if (Ipv4Address::IsMatchingType(m_peerAddress))
  {
    m_socket->Bind();
  }
  else if (Ipv6Address::IsMatchingType(m_peerAddress))
  {
    m_socket->Bind6();
  }

  m_socket->Connect(m_peerAddress);
  m_socket->SetAllowBroadcast(true);
  m_socket->SetRecvCallback(MakeCallback(&UdpClient::SocketReceive, this));

  SendPacket();
}

void
UdpClient::StopApplication(void)
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

void
UdpClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_numPackets)
  {
    m_sendEvent = Simulator::Schedule(m_interPacketInterval, &UdpClient::SendPacket, this);
  }
}

void
UdpClient::SocketReceive(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
  }
}

void
UdpClient::ChangeRate(DataRate newRate)
{
  m_dataRate = newRate;
  m_interPacketInterval = Time(Seconds(8 * (double)m_packetSize / m_dataRate.GetBitRate()));
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
  void SocketReceive(Ptr<Socket> socket);
  void AcceptCallback(Ptr<Socket> socket, const Address &from);
  void CloseCallback(Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_running;
};

UdpServer::UdpServer() : m_port(0), m_socket(nullptr), m_running(false) {}

UdpServer::~UdpServer()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
  }
}

void
UdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void
UdpServer::StartApplication(void)
{
  m_running = true;
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket(GetNode(), tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpServer::SocketReceive, this));
}

void
UdpServer::StopApplication(void)
{
  m_running = false;
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    uint8_t *buffer = new uint8_t[packet->GetSize()];
    packet->CopyData(buffer, packet->GetSize());
    std::cout << "Server received " << packet->GetSize() << " bytes from "
              << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
              << InetSocketAddress::ConvertFrom(from).GetPort() << std::endl;
    delete[] buffer;
  }
}

void
UdpServer::SocketReceive(Ptr<Socket> socket)
{
  HandleRead(socket);
}

void
UdpServer::AcceptCallback(Ptr<Socket> socket, const Address &from)
{
}

void
UdpServer::CloseCallback(Ptr<Socket> socket)
{
}

int main(int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if all packets are received", verbose);
  cmd.AddValue("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  Ptr<UdpServer> serverApp = StaticCast<UdpServer>(serverApps.Get(0));
  serverApp->Setup(port);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  Ptr<UdpClient> clientApp = StaticCast<UdpClient>(clientApps.Get(0));

  InetSocketAddress serverAddress(interfaces.GetAddress(1), port);
  clientApp->Setup(serverAddress, packetSize, numPackets, DataRate("1Mb/s"));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}