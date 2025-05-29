#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/ipv4-address.h"

using namespace ns3;

class CustomUdpServer : public Application
{
public:
  CustomUdpServer() : m_socket(nullptr) {}
  virtual ~CustomUdpServer() { m_socket = nullptr; }
  void Setup(Address address)
  {
    m_localAddress = address;
  }

private:
  virtual void StartApplication() override
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Bind(m_localAddress);
      m_socket->SetRecvCallback(MakeCallback(&CustomUdpServer::HandleRead, this));
    }
  }
  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
      m_socket = nullptr;
    }
  }
  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    if (packet)
    {
      NS_LOG_INFO("Server received packet of size " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << "s");
    }
  }
  Ptr<Socket> m_socket;
  Address m_localAddress;
};

class CustomUdpClient : public Application
{
public:
  CustomUdpClient()
    : m_socket(nullptr),
      m_peerAddress(),
      m_packetSize(1024),
      m_nPackets(10),
      m_interval(Seconds(1.0)),
      m_sendCount(0)
  {}

  virtual ~CustomUdpClient() { m_socket = nullptr; }
  
  void Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
  {
    m_peerAddress = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

private:
  virtual void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peerAddress);
    m_sendCount = 0;
    SendPacket();
  }
  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }
  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    ++m_sendCount;
    if (m_sendCount < m_nPackets)
    {
      Simulator::Schedule(m_interval, &CustomUdpClient::SendPacket, this);
    }
  }
  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sendCount;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("CustomUdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9000;
  Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  Address clientAddress(InetSocketAddress(Ipv4Address::GetAny(), port));

  Ptr<CustomUdpServer> serverApp = CreateObject<CustomUdpServer>();
  serverApp->Setup(clientAddress);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  Ptr<CustomUdpClient> clientApp = CreateObject<CustomUdpClient>();
  clientApp->Setup(serverAddress, 512, 10, Seconds(1.0));
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}