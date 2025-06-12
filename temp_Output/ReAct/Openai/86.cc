#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiSenderApp : public Application
{
public:
  WifiSenderApp ();
  virtual ~WifiSenderApp ();

  void SetDestAddress (Ipv4Address address);
  void SetDestPort (uint16_t port);
  void SetPacketSize (uint32_t size);
  void SetNumPackets (uint32_t n);
  void SetInterval (Time interval);

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void ScheduleTx ();
  void SendPacket ();
  Ptr<Socket>     m_socket;
  Ipv4Address     m_peerAddress;
  uint16_t        m_peerPort;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  uint32_t        m_packetsSent;
  Time            m_interval;
};

WifiSenderApp::WifiSenderApp ()
  : m_socket (0),
    m_peerAddress (),
    m_peerPort (0),
    m_sendEvent (),
    m_running (false),
    m_packetSize (1024),
    m_nPackets (100),
    m_packetsSent (0),
    m_interval (Seconds (1.0))
{
}

WifiSenderApp::~WifiSenderApp ()
{
  m_socket = 0;
}

void
WifiSenderApp::SetDestAddress (Ipv4Address address)
{
  m_peerAddress = address;
}

void
WifiSenderApp::SetDestPort (uint16_t port)
{
  m_peerPort = port;
}

void
WifiSenderApp::SetPacketSize (uint32_t size)
{
  m_packetSize = size;
}

void
WifiSenderApp::SetNumPackets (uint32_t n)
{
  m_nPackets = n;
}

void
WifiSenderApp::SetInterval (Time interval)
{
  m_interval = interval;
}

void
WifiSenderApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress remote = InetSocketAddress (m_peerAddress, m_peerPort);
      m_socket->Connect (remote);
    }
  SendPacket ();
}

void
WifiSenderApp::StopApplication (void)
{
  m_running = false;
  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
WifiSenderApp::SendPacket ()
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);

  // Add current time as a custom packet tag
  struct TimestampTag : public Tag
  {
    Time m_txTime;
    static TypeId GetTypeId (void)
    {
      static TypeId tid = TypeId ("TimestampTag")
                          .SetParent<Tag> ()
                          .AddConstructor<TimestampTag> ();
      return tid;
    }
    TypeId GetInstanceTypeId (void) const override { return GetTypeId (); }
    uint32_t GetSerializedSize (void) const override { return 8; }
    void Serialize (TagBuffer i) const override { i.WriteU64 (m_txTime.GetNanoSeconds ()); }
    void Deserialize (TagBuffer i) override { m_txTime = NanoSeconds (i.ReadU64 ()); }
    void Print (std::ostream &os) const override { os << "tx=" << m_txTime; }
  } tag;

  tag.m_txTime = Simulator::Now ();
  packet->AddPacketTag (tag);

  m_socket->Send (packet);

  ++m_packetsSent;
  if (m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
WifiSenderApp::ScheduleTx ()
{
  if (m_running)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &WifiSenderApp::SendPacket, this);
    }
}

class WifiReceiverApp : public Application
{
public:
  WifiReceiverApp ();
  virtual ~WifiReceiverApp ();

  void SetListeningPort (uint16_t port);

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  uint16_t        m_listenPort;
  uint32_t        m_packetsReceived;
  Time            m_totalDelay;
};

WifiReceiverApp::WifiReceiverApp ()
  : m_socket (0),
    m_listenPort (9999),
    m_packetsReceived (0),
    m_totalDelay (Seconds (0))
{
}

WifiReceiverApp::~WifiReceiverApp ()
{
  m_socket = 0;
}

void
WifiReceiverApp::SetListeningPort (uint16_t port)
{
  m_listenPort = port;
}

void
WifiReceiverApp::StartApplication (void)
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_listenPort);
      m_socket->Bind (local);
    }
  m_socket->SetRecvCallback (MakeCallback (&WifiReceiverApp::HandleRead, this));
  m_packetsReceived = 0;
  m_totalDelay = Seconds (0);
}

void
WifiReceiverApp::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
WifiReceiverApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      m_packetsReceived++;

      // Try to extract timestamp tag
      struct TimestampTag : public Tag
      {
        Time m_txTime;
        static TypeId GetTypeId (void)
        {
          static TypeId tid = TypeId ("TimestampTag")
                              .SetParent<Tag> ()
                              .AddConstructor<TimestampTag> ();
          return tid;
        }
        TypeId GetInstanceTypeId (void) const override { return GetTypeId (); }
        uint32_t GetSerializedSize (void) const override { return 8; }
        void Serialize (TagBuffer i) const override { i.WriteU64 (m_txTime.GetNanoSeconds ()); }
        void Deserialize (TagBuffer i) override { m_txTime = NanoSeconds (i.ReadU64 ()); }
        void Print (std::ostream &os) const override { os << "tx=" << m_txTime; }
      } tag;
      bool found = packet->PeekPacketTag (tag);
      Time rx = Simulator::Now ();
      Time tx = tag.m_txTime;
      Time delay = Time (0);
      if (found)
        {
          delay = rx - tx;
          m_totalDelay += delay;
        }
      std::ostringstream oss;
      oss << "At " << rx.GetSeconds () << "s: Received packet "
          << m_packetsReceived
          << " from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
          << " (delay=" << delay.GetMicroSeconds () << "us)";
      NS_LOG_UNCOND (oss.str ());
    }
}

int
main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 20;
  double interval = 0.5;
  uint16_t port = 9999;
  double simTime = 15.0;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of application packets (bytes)", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets (s)", interval);
  cmd.AddValue ("port", "UDP destination port", port);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.Parse (argc, argv);

  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnable ("UdpSocketImpl", LOG_WARN);
  LogComponentEnable ("WifiSenderApp", LOG_LEVEL_INFO);
  LogComponentEnable ("WifiReceiverApp", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, nodes.Get (0));

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices = wifi.Install (phy, mac, nodes.Get (1));

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (staDevices, apDevices));

  Ptr<WifiSenderApp> sender = CreateObject<WifiSenderApp> ();
  sender->SetPacketSize (packetSize);
  sender->SetNumPackets (numPackets);
  sender->SetInterval (Seconds (interval));
  sender->SetDestAddress (interfaces.GetAddress (1));
  sender->SetDestPort (port);
  nodes.Get (0)->AddApplication (sender);
  sender->SetStartTime (Seconds (1.0));
  sender->SetStopTime (Seconds (simTime));

  Ptr<WifiReceiverApp> receiver = CreateObject<WifiReceiverApp> ();
  receiver->SetListeningPort (port);
  nodes.Get (1)->AddApplication (receiver);
  receiver->SetStartTime (Seconds (0.5));
  receiver->SetStopTime (Seconds (simTime));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}