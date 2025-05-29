#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiver");

class WifiSender : public Application
{
public:
  WifiSender();
  virtual ~WifiSender();

  static TypeId GetTypeId();
  void Setup(Address address, uint16_t port, uint32_t packetSize, uint32_t numPackets, Time interval);

private:
  virtual void StartApplication();
  virtual void StopApplication();

  void SendPacket();
  void ScheduleTx();

  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  Time m_interval;
  Socket *m_socket;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
};

WifiSender::WifiSender()
  : m_peerPort(0),
    m_packetSize(0),
    m_numPackets(0),
    m_interval(Seconds(0)),
    m_socket(nullptr),
    m_packetsSent(0)
{
}

WifiSender::~WifiSender()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
    Simulator::Destroy();
  }
}

TypeId WifiSender::GetTypeId()
{
  static TypeId tid = TypeId("WifiSender")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<WifiSender>()
                            .AddAttribute("Remote",
                                          "The address of the destination",
                                          AddressValue(),
                                          MakeAddressAccessor(&WifiSender::m_peerAddress),
                                          MakeAddressChecker())
                            .AddAttribute("RemotePort", "The port on the destination node to which we will transmit.",
                                          Uinteger16Value(9),
                                          MakeUinteger16Accessor(&WifiSender::m_peerPort),
                                          MakeUinteger16Checker())
                            .AddAttribute("PacketSize", "Size of packets to send",
                                          Uinteger32Value(1024),
                                          MakeUinteger32Accessor(&WifiSender::m_packetSize),
                                          MakeUinteger32Checker())
                            .AddAttribute("NumPackets", "Number of packets to send",
                                          Uinteger32Value(100),
                                          MakeUinteger32Accessor(&WifiSender::m_numPackets),
                                          MakeUinteger32Checker())
                            .AddAttribute("Interval", "Time between packets",
                                          TimeValue(Seconds(1)),
                                          MakeTimeAccessor(&WifiSender::m_interval),
                                          MakeTimeChecker());
  return tid;
}

void WifiSender::Setup(Address address, uint16_t port, uint32_t packetSize, uint32_t numPackets, Time interval)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_interval = interval;
}

void WifiSender::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket == nullptr)
  {
    NS_FATAL_ERROR("Failed to create socket");
  }
  m_socket->Bind();
  m_socket->Connect(m_peerAddress);
  m_packetsSent = 0;
  ScheduleTx();
}

void WifiSender::StopApplication()
{
  Simulator::Cancel(m_sendEvent);
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void WifiSender::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_numPackets)
  {
    ScheduleTx();
  }
}

void WifiSender::ScheduleTx()
{
  m_sendEvent = Simulator::Schedule(m_interval, &WifiSender::SendPacket, this);
}

class WifiReceiver : public Application
{
public:
  WifiReceiver();
  virtual ~WifiReceiver();

  static TypeId GetTypeId();
  void Setup(uint16_t port);

private:
  virtual void StartApplication();
  virtual void StopApplication();

  void HandleRead(Ptr<Socket> socket);
  void ReceivePacket(Ptr<Socket> socket);

  uint16_t m_port;
  Socket *m_socket;
  Ipv4InterfaceAddress m_iface;
  uint64_t m_totalRx;
};

WifiReceiver::WifiReceiver() : m_port(0), m_socket(nullptr), m_totalRx(0) {}

WifiReceiver::~WifiReceiver()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
    Simulator::Destroy();
  }
}

TypeId WifiReceiver::GetTypeId()
{
  static TypeId tid = TypeId("WifiReceiver")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<WifiReceiver>()
                            .AddAttribute("Port", "Port number on which to listen",
                                          Uinteger16Value(9),
                                          MakeUinteger16Accessor(&WifiReceiver::m_port),
                                          MakeUinteger16Checker());
  return tid;
}

void WifiReceiver::Setup(uint16_t port)
{
  m_port = port;
}

void WifiReceiver::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  if (m_socket == nullptr)
  {
    NS_FATAL_ERROR("Failed to create socket");
  }

  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  if (m_socket->Bind(local) == -1)
  {
    NS_FATAL_ERROR("Failed to bind socket");
  }

  m_socket->SetRecvCallback(MakeCallback(&WifiReceiver::HandleRead, this));
  m_socket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &> ());
  m_socket->SetCloseCallbacks(MakeNullCallback<void, Ptr<Socket> > (),
                               MakeNullCallback<void, Ptr<Socket> > ());
  m_socket->Listen();
}

void WifiReceiver::StopApplication()
{
  if (m_socket != nullptr)
  {
    m_socket->Close();
  }
}

void WifiReceiver::HandleRead(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
  {
    Time arrivalTime = Simulator::Now();
    uint32_t packetSize = packet->GetSize();
    m_totalRx += packetSize;

    NS_LOG_INFO("Received " << packetSize << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " at time " << arrivalTime);
  }
}

int main(int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWifi = 2;
  double simulationTime = 10;
  std::string phyMode("DsssRate1Mbps");
  double distance = 1.0;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue("simulationTime", "Simulation runtime in seconds", simulationTime);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("WifiSenderReceiver", LOG_LEVEL_INFO);
  }

  Config::SetDefault("ns3::Wifi80211pHelper::SupportedFrequencies", StringValue("5890000000,5900000000"));
  Config::SetDefault("ns3::Wifi80211pHelper::ChannelNumber", UintegerValue(178));

  NodeContainer wifiNodes;
  wifiNodes.Create(nWifi);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-default");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  interfaces.Add(address.Assign(apDevices));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  Ptr<WifiSender> appSource = CreateObject<WifiSender>();
  appSource->Setup(sinkAddress, port, 1024, 100, Seconds(0.1));
  wifiNodes.Get(0)->AddApplication(appSource);
  appSource->SetStartTime(Seconds(1.0));
  appSource->SetStopTime(Seconds(simulationTime));

  Ptr<WifiReceiver> appSink = CreateObject<WifiReceiver>();
  appSink->Setup(port);
  wifiNodes.Get(1)->AddApplication(appSink);
  appSink->SetStartTime(Seconds(0.0));
  appSink->SetStopTime(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}