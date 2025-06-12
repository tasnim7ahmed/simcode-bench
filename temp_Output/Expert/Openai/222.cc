#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerApp : public Application
{
public:
  UdpServerApp() : m_socket(0) {}
  virtual ~UdpServerApp() { m_socket = 0; }

  void Setup(Address address)
  {
    m_local = address;
  }

private:
  virtual void StartApplication(void)
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Bind(m_local);
      m_socket->SetRecvCallback(MakeCallback(&UdpServerApp::HandleRead, this));
    }
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      uint32_t received = packet->GetSize();
      NS_LOG_UNCOND("At " << Simulator::Now().GetSeconds()
                    << "s server received " << received
                    << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    }
  }

  Ptr<Socket> m_socket;
  Address m_local;
};

class UdpClientApp : public Application
{
public:
  UdpClientApp() :
    m_socket(0),
    m_peer(),
    m_packetSize(0),
    m_nPackets(0),
    m_interval(),
    m_sent(0)
  {}
  virtual ~UdpClientApp() { m_socket = 0; }

  void Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
  {
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

private:
  virtual void StartApplication(void)
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Connect(m_peer);
    }
    m_sent = 0;
    SendPacket();
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
      m_socket->Close();
  }

  void SendPacket(void)
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
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
};

int main(int argc, char *argv[])
{
  LogComponentEnableAll(LOG_PREFIX_TIME);
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi-udp");

  mac.SetType("ns3::StaWifiMac",
      "Ssid", SsidValue(ssid),
      "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
      "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
      "MinX", DoubleValue(0.0),
      "MinY", DoubleValue(0.0),
      "DeltaX", DoubleValue(5.0),
      "DeltaY", DoubleValue(5.0),
      "GridWidth", UintegerValue(3),
      "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  uint16_t port = 8000;
  Address serverAddress(InetSocketAddress(apInterface.GetAddress(0), port));

  // Server App on AP
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp>();
  serverApp->Setup(Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  wifiApNode.Get(0)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  // Client App on STA
  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp>();
  clientApp->Setup(serverAddress, 1024, 100, MilliSeconds(100));
  wifiStaNodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}