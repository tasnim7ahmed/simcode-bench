#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/log.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

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
  Ptr<Socket> m_socket;
  Address m_localAddress;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
  bool m_running;
};

UdpClient::UdpClient() :
  m_packetSize(0),
  m_numPackets(0),
  m_dataRate(0),
  m_socket(nullptr),
  m_packetsSent(0),
  m_running(false)
{
}

UdpClient::~UdpClient()
{
  if (m_socket)
  {
    m_socket->Close();
  }
}

void
UdpClient::Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void
UdpClient::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket->Bind() == -1)
  {
    NS_FATAL_ERROR("Failed to bind socket");
  }
  m_localAddress = m_socket->GetLocalAddress();
  m_socket->Connect(m_peerAddress);
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

  if (m_socket)
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
  if (m_running)
  {
    Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
    m_sendEvent = Simulator::Schedule(tNext, &UdpClient::SendPacket, this);
  }
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
  void Report(void);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_running;
  uint32_t m_totalPacketsReceived;
};

UdpServer::UdpServer() :
  m_port(0),
  m_socket(nullptr),
  m_running(false),
  m_totalPacketsReceived(0)
{
}

UdpServer::~UdpServer()
{
  if (m_socket)
  {
    m_socket->Close();
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
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  if (m_socket->Bind(local) == -1)
  {
    NS_FATAL_ERROR("Failed to bind socket");
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void
UdpServer::StopApplication(void)
{
  m_running = false;
  if (m_socket)
  {
    m_socket->Close();
  }

  Report();
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    m_totalPacketsReceived++;
    NS_LOG_INFO("Received one packet from " << from << " at time " << Simulator::Now().GetSeconds());
  }
}

void
UdpServer::Report(void)
{
  std::cout << "Total packets received: " << m_totalPacketsReceived << std::endl;
}

int
main(int argc, char* argv[])
{
  LogComponentEnable("WifiUdpCustom", LOG_LEVEL_INFO);

  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  DataRate dataRate("5Mbps");
  std::string phyMode("DsssRate11Mbps");

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("dataRate", "Sending rate in bps", dataRate);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNodes;
  staNodes.Create(1);

  NodeContainer serverNode;
  serverNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconGeneration", BooleanValue(true),
                  "BeaconInterval", TimeValue(Seconds(1.0)));

  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, staNodes);

  NetDeviceContainer serverDevice = wifi.Install(wifiPhy, wifiMac, serverNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(apNode);
  mobility.Install(staNodes);
  mobility.Install(serverNode);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);
  internet.Install(serverNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(apDevice);
  Ipv4InterfaceContainer j = ipv4.Assign(staDevice);
  Ipv4InterfaceContainer k = ipv4.Assign(serverDevice);

  Ipv4Address serverAddress = k.GetAddress(0);
  uint16_t port = 9;

  UdpServerHelper echoServerHelper(port);
  ApplicationContainer serverApps = echoServerHelper.Install(serverNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  Address serverAddr(InetSocketAddress(serverAddress, port));
  UdpClient client;
  client.Setup(serverAddr, packetSize, numPackets, dataRate);
  staNodes.Get(0)->AddApplication(&client);
  client.SetStartTime(Seconds(2.0));
  client.SetStopTime(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}