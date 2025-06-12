#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiSenderReceiverExample");

// -------------------------------
// WiFi Sender Application
// -------------------------------

class WiFiSenderApp : public Application
{
public:
  WiFiSenderApp();
  virtual ~WiFiSenderApp();

  void SetRemote(Address address, uint16_t port);
  void SetPacketSize(uint32_t size);
  void SetNumPackets(uint32_t count);
  void SetInterval(Time interval);

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void ScheduleTx();
  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
  Time m_interval;
};

WiFiSenderApp::WiFiSenderApp()
  : m_socket(0),
    m_peerPort(0),
    m_packetSize(1024),
    m_nPackets(10),
    m_packetsSent(0),
    m_interval(Seconds(1.0))
{
}

WiFiSenderApp::~WiFiSenderApp()
{
  m_socket = 0;
}

void
WiFiSenderApp::SetRemote(Address address, uint16_t port)
{
  m_peerAddress = address;
  m_peerPort = port;
}

void
WiFiSenderApp::SetPacketSize(uint32_t size)
{
  m_packetSize = size;
}

void
WiFiSenderApp::SetNumPackets(uint32_t count)
{
  m_nPackets = count;
}

void
WiFiSenderApp::SetInterval(Time interval)
{
  m_interval = interval;
}

void
WiFiSenderApp::StartApplication()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(
        GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
    }
  m_packetsSent = 0;
  SendPacket();
}

void
WiFiSenderApp::StopApplication()
{
  if (m_socket)
    {
      m_socket->Close();
    }
  Simulator::Cancel(m_sendEvent);
}

void
WiFiSenderApp::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);

  // Add send timestamp as header for delay estimation
  Time now = Simulator::Now();
  uint64_t timestamp = now.GetNanoSeconds();
  packet->AddPaddingAtEnd(8); // make sure space is there for user header

  // Replace first 8 bytes with timestamp
  uint8_t buf[8];
  for(int i = 0; i < 8; ++i)
    buf[i] = (timestamp >> (i * 8)) & 0xFF;
  packet->RemoveAtStart(8);
  packet->AddAtStart(Create<Packet>(buf,8));

  m_socket->Send(packet);

  m_packetsSent++;
  if (m_packetsSent < m_nPackets)
    {
      ScheduleTx();
    }
}

void
WiFiSenderApp::ScheduleTx()
{
  m_sendEvent = Simulator::Schedule(m_interval, &WiFiSenderApp::SendPacket, this);
}

// -------------------------------
// WiFi Receiver Application
// -------------------------------

class WiFiReceiverApp : public Application
{
public:
  WiFiReceiverApp();
  virtual ~WiFiReceiverApp();

  void SetListenPort(uint16_t port);

  double GetMeanDelay() const;   // in ms
  double GetMaxDelay() const;

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void HandleRead(Ptr<Socket> socket);

  uint16_t m_listenPort;
  Ptr<Socket> m_socket;
  uint32_t m_packetsReceived;
  double m_totalDelay; // Accumulated delay in milliseconds
  double m_maxDelay;   // Maximum observed delay in ms
};

WiFiReceiverApp::WiFiReceiverApp()
  : m_listenPort(0),
    m_socket(0),
    m_packetsReceived(0),
    m_totalDelay(0.0),
    m_maxDelay(0.0)
{
}

WiFiReceiverApp::~WiFiReceiverApp()
{
  m_socket = 0;
}

void
WiFiReceiverApp::SetListenPort(uint16_t port)
{
  m_listenPort = port;
}

void
WiFiReceiverApp::StartApplication()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(
        GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_listenPort);
      m_socket->Bind(local);
    }

  m_socket->SetRecvCallback(
    MakeCallback(&WiFiReceiverApp::HandleRead, this));
}

void
WiFiReceiverApp::StopApplication()
{
  if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
    }
}

void
WiFiReceiverApp::HandleRead(Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
      Time now = Simulator::Now();
      if (packet->GetSize() >= 8)
        {
          // Extract timestamp from first 8 bytes
          uint8_t buf[8];
          packet->CopyData(buf, 8);

          uint64_t sentNs =
            (((uint64_t)buf[7]) << 56) | (((uint64_t)buf[6]) << 48) |
            (((uint64_t)buf[5]) << 40) | (((uint64_t)buf[4]) << 32) |
            (((uint64_t)buf[3]) << 24) | (((uint64_t)buf[2]) << 16) |
            (((uint64_t)buf[1]) << 8)  | (((uint64_t)buf[0]));

          Time sentTime = NanoSeconds(sentNs);
          double delayMs = (now - sentTime).GetMilliSeconds();
          m_totalDelay += delayMs;
          if (delayMs > m_maxDelay) m_maxDelay = delayMs;

          NS_LOG_INFO("Packet received: " <<
              " size=" << packet->GetSize() <<
              " from=" << InetSocketAddress::ConvertFrom(from).GetIpv4() <<
              " at time=" << now.GetSeconds() << "s" <<
              " delay=" << std::fixed << std::setprecision(3) << delayMs << " ms");
        }
      else
        {
          NS_LOG_WARN("Received packet too small for delay calculation");
        }
      m_packetsReceived++;
    }
}

double
WiFiReceiverApp::GetMeanDelay() const
{
  if (m_packetsReceived == 0) return 0.0;
  return m_totalDelay / m_packetsReceived;
}

double
WiFiReceiverApp::GetMaxDelay() const
{
  return m_maxDelay;
}

// -------------------------------
// Main Simulation
// -------------------------------

int
main(int argc, char *argv[])
{
  LogComponentEnable("WiFiSenderReceiverExample", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t nPackets = 20;
  uint32_t packetSize = 1024;
  double interval = 0.5; // seconds
  uint16_t port = 50000;

  CommandLine cmd;
  cmd.AddValue("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue("packetSize", "Size of each packet (bytes)", packetSize);
  cmd.AddValue("interval", "Interval between packets (s)", interval);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns3-wifi-apps");
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

  // Set mobility, fixed positions
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(0.0),
                               "GridWidth", UintegerValue(2),
                               "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(staDevice, apDevice));

  // Install applications
  Ptr<WiFiSenderApp> sender = CreateObject<WiFiSenderApp>();
  sender->SetRemote(interfaces.GetAddress(1), port);
  sender->SetPacketSize(packetSize);
  sender->SetNumPackets(nPackets);
  sender->SetInterval(Seconds(interval));
  nodes.Get(0)->AddApplication(sender);
  sender->SetStartTime(Seconds(1.0));
  sender->SetStopTime(Seconds(1.0 + nPackets * interval + 1.0));

  Ptr<WiFiReceiverApp> receiver = CreateObject<WiFiReceiverApp>();
  receiver->SetListenPort(port);
  nodes.Get(1)->AddApplication(receiver);
  receiver->SetStartTime(Seconds(0.0));
  receiver->SetStopTime(Seconds(1.0 + nPackets * interval + 2.0));

  Simulator::Stop(Seconds(1.0 + nPackets * interval + 4.0));
  Simulator::Run();

  uint32_t totalRx = receiver->GetObject<WiFiReceiverApp>()->m_packetsReceived;
  double meanDelay = receiver->GetMeanDelay();
  double maxDelay = receiver->GetMaxDelay();

  std::cout << "*** Simulation Summary ***\n";
  std::cout << "Packets sent    : " << nPackets << std::endl;
  std::cout << "Packets received: " << totalRx << std::endl;
  std::cout << "Mean delay      : " << std::fixed << std::setprecision(3) << meanDelay << " ms" << std::endl;
  std::cout << "Max delay       : " << std::fixed << std::setprecision(3) << maxDelay << " ms" << std::endl;

  Simulator::Destroy();
  return 0;
}