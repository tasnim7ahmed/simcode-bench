#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpCustom");

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
  void ScheduleTx(void);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Socket m_socket;
  Time m_interPacketInterval;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient()
  : m_packetSize(0),
    m_numPackets(0),
    m_dataRate(0),
    m_socket(nullptr),
    m_interPacketInterval(Seconds(0)),
    m_sendEvent(),
    m_packetsSent(0)
{
}

UdpClient::~UdpClient()
{
  m_socket = nullptr;
}

void
UdpClient::Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = Time(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
}

void
UdpClient::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket == nullptr) {
    std::cerr << "Failed to create socket" << std::endl;
    return;
  }
  m_socket->Connect(m_peerAddress);
  m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>, Ptr<Packet>, const Address &>());
  m_packetsSent = 0;
  ScheduleTx();
}

void
UdpClient::StopApplication(void)
{
  Simulator::Cancel(m_sendEvent);
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
      ScheduleTx();
    }
}

void
UdpClient::ScheduleTx(void)
{
  m_sendEvent = Simulator::Schedule(m_interPacketInterval, &UdpClient::SendPacket, this);
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

  uint16_t m_port;
  Socket m_socket;
  bool m_running;
};

UdpServer::UdpServer()
  : m_port(0),
    m_socket(nullptr),
    m_running(false)
{
}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

void
UdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void
UdpServer::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket == nullptr) {
    std::cerr << "Failed to create socket" << std::endl;
    return;
  }
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  if (m_socket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }

  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));

  m_running = true;
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
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
    {
      std::cout << "Received one packet from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
    }
}

int
main(int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  double interval = 0.01;
  std::string dataRate = "1Mbps";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("packetSize", "Size of echo packets", packetSize);
  cmd.AddValue("numPackets", "Number of packets sent", numPackets);
  cmd.AddValue("interval", "Interval between packets", interval);
  cmd.AddValue("dataRate", "Data rate of the client", dataRate);
  cmd.Parse(argc, argv);

  if (verbose)
    {
      LogComponentEnable("WifiUdpCustom", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create(1);

  NodeContainer apNodes;
  apNodes.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac",
                   "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(wifiPhy, wifiMac, apNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(apNodes);
  mobility.Install(staNodes);

  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(apDevices);
  Ipv4InterfaceContainer j = ipv4.Assign(staDevices);

  Ipv4Address serverAddress = i.GetAddress(0);
  uint16_t serverPort = 9;

  UdpServerHelper echoServer(serverPort);

  ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  Ptr<UdpClient> clientApp = CreateObject<UdpClient>();
  clientApp->Setup(InetSocketAddress(serverAddress, serverPort), packetSize, numPackets, DataRate(dataRate));
  staNodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(2.0));
  clientApp->SetStopTime(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}