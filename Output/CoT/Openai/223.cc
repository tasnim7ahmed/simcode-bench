#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocUdpExample");

class UdpServerApp : public Application
{
public:
  UdpServerApp() : m_socket(0) {}
  virtual ~UdpServerApp() { m_socket = 0; }

  void Setup(Address address, uint16_t port)
  {
    m_local = address;
    m_port = port;
  }

private:
  virtual void StartApplication()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::ConvertFrom(m_local), m_port);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
    }
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      uint32_t pktSize = packet->GetSize();
      double now = Simulator::Now().GetSeconds();
      std::cout << "At " << now << "s server received " << pktSize << " bytes from "
                << InetSocketAddress::ConvertFrom(from).GetIpv4() << std::endl;
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
  uint16_t m_port;
};

class UdpClientApp : public Application
{
public:
  UdpClientApp() 
    : m_socket(0),
      m_peerAddress(),
      m_peerPort(0),
      m_packetSize(0),
      m_nPackets(0),
      m_interval(),
      m_sent(0) 
  {}

  virtual ~UdpClientApp() { m_socket = 0; }

  void Setup(Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval)
  {
    m_peerAddress = address;
    m_peerPort = port;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
    m_sent = 0;
    SendPacket();
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    ++m_sent;
    if (m_sent < m_nPackets)
    {
      Simulator::Schedule(m_interval, &UdpClientApp::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper chan = YansWifiChannelHelper::Default();
  phy.SetChannel(chan.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  uint16_t udpPort = 4000;

  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp>();
  serverApp->Setup(interfaces.GetAddress(1), udpPort);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  uint32_t pktSize = 1024;
  double interval = 1.0;
  uint32_t nPackets = 10;

  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp>();
  clientApp->Setup(interfaces.GetAddress(1), udpPort, pktSize, nPackets, Seconds(interval));
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}