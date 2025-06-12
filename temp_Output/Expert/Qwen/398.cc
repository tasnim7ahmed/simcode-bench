#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpSimulation");

class TcpClient : public Application
{
public:
  static TypeId GetTypeId();
  TcpClient();
  virtual ~TcpClient();

protected:
  void StartApplication() override;
  void StopApplication() override;

private:
  void SendPacket();
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  Time m_interval;
  EventId m_sendEvent;
};

TypeId TcpClient::GetTypeId()
{
  static TypeId tid = TypeId("TcpClient")
                          .SetParent<Application>()
                          .AddConstructor<TcpClient>()
                          .AddAttribute("RemoteAddress", "The destination Address of the server",
                                        AddressValue(), MakeAddressAccessor(&TcpClient::m_peer),
                                        MakeAddressChecker())
                          .AddAttribute("PacketSize", "Size of the packet to send (bytes)",
                                        UintegerValue(1024), MakeUintegerAccessor(&TcpClient::m_packetSize),
                                        MakeUintegerChecker<uint32_t>())
                          .AddAttribute("Interval", "Time interval between packets",
                                        TimeValue(MilliSeconds(100)), MakeTimeAccessor(&TcpClient::m_interval),
                                        MakeTimeChecker());
  return tid;
}

TcpClient::TcpClient()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(1024),
      m_interval(MilliSeconds(100)),
      m_sendEvent()
{
}

TcpClient::~TcpClient()
{
  m_socket = nullptr;
}

void TcpClient::StartApplication()
{
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
  }
  SendPacket();
}

void TcpClient::StopApplication()
{
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
  if (m_socket)
  {
    m_socket->Close();
  }
}

void TcpClient::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  m_sendEvent = Simulator::Schedule(m_interval, &TcpClient::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"),
                               "ControlMode", StringValue("HtMcs0"));

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  // Server application
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Client application
  Address sinkRemoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpReno::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

  TcpClient client;
  client.SetAttribute("RemoteAddress", AddressValue(sinkRemoteAddress));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));

  ApplicationContainer clientApp;
  clientApp.Add(CreateObject<TcpClient>());
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}