#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpWifiSimulation");

class UdpServer : public Application
{
public:
  UdpServer();
  virtual ~UdpServer();

  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, uint16_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

UdpServer::UdpServer()
    : m_socket(0), m_port(0)
{
}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

TypeId UdpServer::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpServer")
                          .SetParent<Application>()
                          .SetGroupName("Applications")
                          .AddConstructor<UdpServer>();
  return tid;
}

void UdpServer::Setup(Ptr<Socket> socket, uint16_t port)
{
  m_socket = socket;
  m_port = port;
}

void UdpServer::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void UdpServer::HandleRead(Ptr<Socket> socket)
{
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
  UdpClient();
  virtual ~UdpClient();

  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint32_t m_packetSize;
  Time m_interval;
  EventId m_sendEvent;
  bool m_running;
};

UdpClient::UdpClient()
    : m_socket(0),
      m_peerAddress(),
      m_packetSize(512),
      m_interval(Seconds(1.0)),
      m_sendEvent(),
      m_running(false)
{
}

UdpClient::~UdpClient()
{
  m_socket = nullptr;
}

TypeId UdpClient::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpClient")
                          .SetParent<Application>()
                          .SetGroupName("Applications")
                          .AddConstructor<UdpClient>();
  return tid;
}

void UdpClient::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval)
{
  m_socket = socket;
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_interval = interval;
}

void UdpClient::StartApplication(void)
{
  m_running = true;
  SendPacket();
}

void UdpClient::StopApplication(void)
{
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

void UdpClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_peerAddress);

  if (m_running)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  NetDeviceContainer staDevice;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("wifi-network")),
              "ActiveProbing", BooleanValue(false));
  staDevice = wifi.Install(phy, mac, wifiStaNode);

  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
  apDevice = wifi.Install(phy, mac, wifiApNode);

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
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevice);

  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign(apDevice);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> serverSocket = Socket::CreateSocket(wifiApNode.Get(0), tid);
  uint16_t serverPort = 9;

  Ptr<UdpServer> serverApp = CreateObject<UdpServer>();
  serverApp->Setup(serverSocket, serverPort);
  wifiApNode.Get(0)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  Ptr<Socket> clientSocket = Socket::CreateSocket(wifiStaNode.Get(0), tid);
  Address serverAddress(InetSocketAddress(apInterfaces.GetAddress(0), serverPort));

  Ptr<UdpClient> clientApp = CreateObject<UdpClient>();
  clientApp->Setup(clientSocket, serverAddress, 1024, MilliSeconds(500));
  wifiStaNode.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}