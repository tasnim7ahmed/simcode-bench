#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpBroadcastExample");

// Application to send UDP broadcast packets
class UdpBroadcastSender : public Application
{
public:
  UdpBroadcastSender();
  virtual ~UdpBroadcastSender();

  void Setup(Address broadcastAddress, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

  void SendPacket();

  Ptr<Socket>     m_socket;
  Address         m_broadcastAddress;
  uint16_t        m_port;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_interval;
  uint32_t        m_sent;
  EventId         m_sendEvent;
};

UdpBroadcastSender::UdpBroadcastSender()
  : m_socket(0),
    m_port(0),
    m_packetSize(0),
    m_nPackets(0),
    m_sent(0)
{
}

UdpBroadcastSender::~UdpBroadcastSender()
{
  m_socket = 0;
}

void
UdpBroadcastSender::Setup(Address broadcastAddress, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_broadcastAddress = broadcastAddress;
  m_port = port;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
}

void
UdpBroadcastSender::StartApplication()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->SetAllowBroadcast(true);
    }
  m_sent = 0;
  SendPacket();
}

void
UdpBroadcastSender::StopApplication()
{
  if (m_socket)
    {
      m_socket->Close();
    }
  Simulator::Cancel(m_sendEvent);
}

void
UdpBroadcastSender::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, InetSocketAddress(InetSocketAddress::ConvertFrom(m_broadcastAddress).GetIpv4(), m_port));
  m_sent++;
  if (m_sent < m_nPackets)
    {
      m_sendEvent = Simulator::Schedule(m_interval, &UdpBroadcastSender::SendPacket, this);
    }
}

// Receiver callback
void ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: Node " 
                      << socket->GetNode()->GetId() 
                      << " received " << packet->GetSize()
                      << " bytes from " << address.GetIpv4());
    }
}

int main(int argc, char *argv[])
{
  uint32_t nReceivers = 3;
  uint32_t packetSize = 128;
  uint16_t port = 8000;
  uint32_t nPackets = 10;
  double interval = 1.0; // seconds

  CommandLine cmd;
  cmd.AddValue("nReceivers", "Number of receiver nodes", nReceivers);
  cmd.AddValue("packetSize", "Size of each packet (bytes)", packetSize);
  cmd.AddValue("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue("interval", "Interval between packets (seconds)", interval);
  cmd.Parse(argc, argv);

  uint32_t nNodes = nReceivers + 1;
  NodeContainer nodes;
  nodes.Create(nNodes);

  // WiFi setup
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("simple-ssid");

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1, nReceivers));

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

  NetDeviceContainer allDevices;
  allDevices.Add(apDevice);
  allDevices.Add(staDevices);

  // No mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      positionAlloc->Add(Vector(5 * i, 0, 0));
    }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

  // Application: UdpBroadcastSender on sender node (nodes.Get(0))
  Ptr<UdpBroadcastSender> senderApp = CreateObject<UdpBroadcastSender>();
  Ipv4Address broadcastAddr("10.1.1.255");
  senderApp->Setup(Address(broadcastAddr), port, packetSize, nPackets, Seconds(interval));
  nodes.Get(0)->AddApplication(senderApp);
  senderApp->SetStartTime(Seconds(1.0));
  senderApp->SetStopTime(Seconds(1.0 + interval * nPackets + 1.0));

  // UDP sockets on receivers
  for (uint32_t i = 1; i < nNodes; ++i)
    {
      Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
      recvSocket->Bind(local);
      recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    }

  Simulator::Stop(Seconds(1.0 + interval * nPackets + 2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}