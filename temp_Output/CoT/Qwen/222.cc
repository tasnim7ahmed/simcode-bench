#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpWifiSimulation");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId();
  UdpServer();
  virtual ~UdpServer();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  void HandleRead(Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
};

TypeId
UdpServer::GetTypeId()
{
  static TypeId tid = TypeId("UdpServer")
    .SetParent<Application>()
    .AddConstructor<UdpServer>()
    .AddAttribute("Port", "Port on which we listen for incoming packets.",
                  UintegerValue(9),
                  MakeUintegerAccessor(&UdpServer::m_port),
                  MakeUintegerChecker<uint16_t>());
  return tid;
}

UdpServer::UdpServer()
{
  m_socket = nullptr;
}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

void
UdpServer::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
  }
}

void
UdpServer::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO("Received packet of size " << packet->GetSize() << " at time " << Simulator::Now().GetSeconds());
  }
}

class UdpClient : public Application
{
public:
  static TypeId GetTypeId();
  UdpClient();
  virtual ~UdpClient();

  void SendPacket();

protected:
  virtual void StartApplication();
  virtual void StopApplication();

private:
  Time m_interval;
  uint32_t m_packetSize;
  Ipv4Address m_serverIp;
  uint16_t m_serverPort;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
};

TypeId
UdpClient::GetTypeId()
{
  static TypeId tid = TypeId("UdpClient")
    .SetParent<Application>()
    .AddConstructor<UdpClient>()
    .AddAttribute("Interval", "Time between consecutive packets",
                  TimeValue(Seconds(1.0)),
                  MakeTimeAccessor(&UdpClient::m_interval),
                  MakeTimeChecker())
    .AddAttribute("PacketSize", "Size of each packet in bytes",
                  UintegerValue(512),
                  MakeUintegerAccessor(&UdpClient::m_packetSize),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("RemoteAddress", "IP address of the destination",
                  Ipv4AddressValue("192.168.1.2"),
                  MakeIpv4AddressAccessor(&UdpClient::m_serverIp),
                  MakeIpv4AddressChecker())
    .AddAttribute("RemotePort", "Destination port",
                  UintegerValue(9),
                  MakeUintegerAccessor(&UdpClient::m_serverPort),
                  MakeUintegerChecker<uint16_t>());
  return tid;
}

UdpClient::UdpClient()
{
  m_socket = nullptr;
}

UdpClient::~UdpClient()
{
  m_socket = nullptr;
  Simulator::Cancel(m_sendEvent);
}

void
UdpClient::StartApplication()
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
  }

  SendPacket();
}

void
UdpClient::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }

  Simulator::Cancel(m_sendEvent);
}

void
UdpClient::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, InetSocketAddress(m_serverIp, m_serverPort));

  NS_LOG_INFO("Sent packet of size " << m_packetSize << " at time " << Simulator::Now().GetSeconds());

  m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer apNode;
  apNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  NetDeviceContainer staDevice;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("wifi-infra")),
              "ActiveProbing", BooleanValue(false));
  staDevice = wifi.Install(phy, mac, wifiStaNode);

  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("wifi-infra")));
  apDevice = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
  mobility.Install(wifiStaNode);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(apNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign(staDevice);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  UdpServerHelper serverHelper(9);
  ApplicationContainer serverApp = serverHelper.Install(apNode.Get(0));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper clientHelper;
  clientHelper.SetAttribute("RemoteAddress", Ipv4AddressValue(apInterface.GetAddress(0)));
  clientHelper.SetAttribute("RemotePort", UintegerValue(9));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApp = clientHelper.Install(wifiStaNode.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}