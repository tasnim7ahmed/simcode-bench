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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpCustomApp");

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

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Socket *m_socket;
  Time m_interPacketInterval;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient()
    : m_socket(nullptr),
      m_packetsSent(0)
{
}

UdpClient::~UdpClient()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
    Simulator::Destroy();
  }
}

void UdpClient::Setup(Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
  m_interPacketInterval = Time(Seconds(8 * (double)packetSize / dataRate.GetBitRate()));
}

void UdpClient::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket == nullptr) {
      std::cerr << "Failed to create socket!" << std::endl;
      exit(1);
  }
  m_socket->Bind();
  m_socket->Connect(m_peerAddress);

  SendPacket();
}

void UdpClient::StopApplication(void)
{
  Simulator::Cancel(m_sendEvent);
  if (m_socket != nullptr)
  {
      m_socket->Close();
  }
}

void UdpClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  m_packetsSent++;

  if (m_packetsSent < m_numPackets)
  {
    m_sendEvent = Simulator::Schedule(m_interPacketInterval, &UdpClient::SendPacket, this);
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
  void ReportRx(Ptr<const Packet> p, const Address &src);

  uint16_t m_port;
  Socket *m_socket;
  bool m_running;
};

UdpServer::UdpServer() : m_socket(nullptr), m_running(false) {}
UdpServer::~UdpServer()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
    Simulator::Destroy();
  }
}

void UdpServer::Setup(uint16_t port)
{
  m_port = port;
}

void UdpServer::StartApplication(void)
{
  m_running = true;
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    if (m_socket == nullptr) {
      std::cerr << "Failed to create socket!" << std::endl;
      exit(1);
  }
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication(void)
{
  m_running = false;
  if (m_socket != nullptr)
  {
      m_socket->Close();
  }
}

void UdpServer::HandleRead(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
  {
    if (packet->GetSize() > 0)
    {
      ReportRx(packet, from);
    }
  }
}

void UdpServer::ReportRx(Ptr<const Packet> p, const Address &src)
{
  std::cout << Simulator::Now().As(Seconds) << " Server received " << p->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(src).GetIpv4() << std::endl;
}

int main(int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  DataRate dataRate("5Mbps");
  std::string phyMode("DsssRate11Mbps");

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue("packetSize", "Size of echo packet", packetSize);
  cmd.AddValue("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue("dataRate", "Data rate", dataRate);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMac::Ssid", SsidValue(Ssid("ns-3-ssid")));

  NodeContainer staNodes;
  staNodes.Create(1);

  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");

  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  NetDeviceContainer staDevices;
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(Ssid("ns-3-ssid")),
                  "ActiveProbing", BooleanValue(false));
  staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  NetDeviceContainer apDevices;
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(Ssid("ns-3-ssid")));
  apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

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

  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(apNode.Get(0));
  Ptr<UdpServer> serverApp = DynamicCast<UdpServer>(serverApps.Get(0));
    if(serverApp == NULL) {
        std::cerr << "Server App install failed" << std::endl;
        exit(1);
    }
  serverApp->Setup(port);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(apInterfaces.GetAddress(0), port);
  ApplicationContainer clientApps = client.Install(staNodes.Get(0));

    Ptr<UdpClient> clientApp = DynamicCast<UdpClient>(clientApps.Get(0));
    if (clientApp == NULL) {
        std::cerr << "Client App install failed" << std::endl;
        exit(1);
    }
  clientApp->Setup(apInterfaces.GetAddress(0), packetSize, numPackets, dataRate);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}