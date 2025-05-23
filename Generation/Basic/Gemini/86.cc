#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/tag.h"
#include <limits> // For std::numeric_limits
#include <algorithm> // For std::min/max

NS_LOG_COMPONENT_DEFINE ("WifiSenderReceiverExample");

// Custom Packet Tag for timestamping
class TimestampTag : public ns3::Tag
{
public:
  static ns3::TypeId GetTypeId ();
  virtual ns3::TypeId GetInstanceTypeId () const;
  virtual uint32_t GetSerializedSize () const;
  virtual void Serialize (ns3::Buffer::Iterator start) const;
  virtual void Deserialize (ns3::Buffer::Iterator start);
  virtual void Print (std::ostream &os) const;

  void SetTimestamp (uint64_t timestamp);
  uint64_t GetTimestamp () const;

private:
  uint64_t m_timestamp; // Microseconds since simulation start
};

ns3::TypeId
TimestampTag::GetTypeId ()
{
  static ns3::TypeId tid = ns3::TypeId ("ns3::TimestampTag")
                               .SetParent<ns3::Tag> ()
                               .AddConstructor<TimestampTag> ()
                               .AddAttribute ("Timestamp",
                                              "Timestamp in microseconds.",
                                              ns3::EmptyAttributeValue (),
                                              ns3::MakeUintegerAccessor (&TimestampTag::m_timestamp),
                                              ns3::MakeUintegerChecker<uint64_t> ());
  return tid;
}

ns3::TypeId
TimestampTag::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TimestampTag::GetSerializedSize () const
{
  return sizeof (uint64_t); // Size of timestamp in bytes
}

void
TimestampTag::Serialize (ns3::Buffer::Iterator start) const
{
  ns3::Buffer::Iterator i = start;
  i.WriteUinteger64 (m_timestamp);
}

void
TimestampTag::Deserialize (ns3::Buffer::Iterator start)
{
  ns3::Buffer::Iterator i = start;
  m_timestamp = i.ReadUinteger64 ();
}

void
TimestampTag::Print (std::ostream &os) const
{
  os << "TimestampTag: " << m_timestamp;
}

void
TimestampTag::SetTimestamp (uint64_t timestamp)
{
  m_timestamp = timestamp;
}

uint64_t
TimestampTag::GetTimestamp () const
{
  return m_timestamp;
}

// Wi-Fi Sender Application
class WifiSenderApp : public ns3::Application
{
public:
  static ns3::TypeId GetTypeId ();
  WifiSenderApp ();
  virtual ~WifiSenderApp ();
  void SetRemote (ns3::Ipv4Address ip, uint16_t port);
  void SetPacketSize (uint32_t size);
  void SetInterval (ns3::Time interval);
  void SetMaxPackets (uint32_t maxPackets);

private:
  virtual void StartApplication ();
  virtual void StopApplication ();
  void SendPacket ();

  ns3::Ptr<ns3::Socket> m_socket;
  ns3::Ipv4Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  ns3::Time m_interval;
  uint32_t m_maxPackets;
  uint32_t m_packetsSent;
  ns3::EventId m_sendEvent;
};

ns3::TypeId
WifiSenderApp::GetTypeId ()
{
  static ns3::TypeId tid = ns3::TypeId ("WifiSenderApp")
                               .SetParent<ns3::Application> ()
                               .AddConstructor<WifiSenderApp> ()
                               .AddAttribute ("PeerAddress",
                                              "The destination IP address of the receiver",
                                              ns3::Ipv4AddressValue (),
                                              ns3::MakeIpv4AddressAccessor (&WifiSenderApp::m_peerAddress),
                                              ns3::MakeIpv4AddressChecker ())
                               .AddAttribute ("PeerPort",
                                              "The destination port of the receiver",
                                              ns3::UintegerValue (9),
                                              ns3::MakeUintegerAccessor (&WifiSenderApp::m_peerPort),
                                              ns3::MakeUintegerChecker<uint16_t> ())
                               .AddAttribute ("PacketSize",
                                              "Size of packets generated.",
                                              ns3::UintegerValue (1000),
                                              ns3::MakeUintegerAccessor (&WifiSenderApp::m_packetSize),
                                              ns3::MakeUintegerChecker<uint32_t> ())
                               .AddAttribute ("Interval",
                                              "The time interval between packets.",
                                              ns3::TimeValue (ns3::Seconds (1.0)),
                                              ns3::MakeTimeAccessor (&WifiSenderApp::m_interval),
                                              ns3::MakeTimeChecker ())
                               .AddAttribute ("MaxPackets",
                                              "The maximum number of packets to send.",
                                              ns3::UintegerValue (100),
                                              ns3::MakeUintegerAccessor (&WifiSenderApp::m_maxPackets),
                                              ns3::MakeUintegerChecker<uint32_t> ());
  return tid;
}

WifiSenderApp::WifiSenderApp ()
  : m_socket (nullptr),
    m_peerAddress (),
    m_peerPort (0),
    m_packetSize (0),
    m_interval (ns3::Time::Zero ()),
    m_maxPackets (0),
    m_packetsSent (0),
    m_sendEvent ()
{
}

WifiSenderApp::~WifiSenderApp ()
{
  m_socket = nullptr; // Ptr will manage memory
}

void
WifiSenderApp::SetRemote (ns3::Ipv4Address ip, uint16_t port)
{
  m_peerAddress = ip;
  m_peerPort = port;
}

void
WifiSenderApp::SetPacketSize (uint32_t size)
{
  m_packetSize = size;
}

void
WifiSenderApp::SetInterval (ns3::Time interval)
{
  m_interval = interval;
}

void
WifiSenderApp::SetMaxPackets (uint32_t maxPackets)
{
  m_maxPackets = maxPackets;
}

void
WifiSenderApp::StartApplication ()
{
  if (m_socket == nullptr)
  {
    m_socket = ns3::Socket::CreateSocket (GetNode (), ns3::UdpSocketFactory::GetTypeId ());
    m_socket->Bind (); // Bind to any address and an ephemeral port
    m_socket->Connect (ns3::InetSocketAddress (m_peerAddress, m_peerPort));
  }

  if (m_maxPackets > 0)
  {
    m_packetsSent = 0;
    m_sendEvent = ns3::Simulator::Schedule (ns3::Seconds (0.0), &WifiSenderApp::SendPacket, this);
  }
}

void
WifiSenderApp::StopApplication ()
{
  ns3::Simulator::Cancel (m_sendEvent);
  if (m_socket != nullptr)
  {
    m_socket->Close ();
  }
}

void
WifiSenderApp::SendPacket ()
{
  NS_LOG_INFO ("Sender: Sending packet " << m_packetsSent + 1 << " at " << ns3::Simulator::Now ());

  ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet> (m_packetSize);
  TimestampTag tag;
  tag.SetTimestamp (ns3::Simulator::Now ().GetMicroSeconds ());
  packet->AddPacketTag (tag);

  m_socket->Send (packet);

  m_packetsSent++;

  if (m_packetsSent < m_maxPackets)
  {
    m_sendEvent = ns3::Simulator::Schedule (m_interval, &WifiSenderApp::SendPacket, this);
  }
  else
  {
    NS_LOG_INFO ("Sender: All " << m_maxPackets << " packets sent.");
  }
}

// Wi-Fi Receiver Application
class WifiReceiverApp : public ns3::Application
{
public:
  static ns3::TypeId GetTypeId ();
  WifiReceiverApp ();
  virtual ~WifiReceiverApp ();
  void SetLocalPort (uint16_t port);

private:
  virtual void StartApplication ();
  virtual void StopApplication ();
  void HandleRead (ns3::Ptr<ns3::Socket> socket);

  ns3::Ptr<ns3::Socket> m_socket;
  uint16_t m_localPort;
  uint32_t m_packetsReceived;
  uint64_t m_totalDelayUs;
  uint64_t m_minDelayUs;
  uint64_t m_maxDelayUs;
};

ns3::TypeId
WifiReceiverApp::GetTypeId ()
{
  static ns3::TypeId tid = ns3::TypeId ("WifiReceiverApp")
                               .SetParent<ns3::Application> ()
                               .AddConstructor<WifiReceiverApp> ()
                               .AddAttribute ("LocalPort",
                                              "The local port to listen on.",
                                              ns3::UintegerValue (9),
                                              ns3::MakeUintegerAccessor (&WifiReceiverApp::m_localPort),
                                              ns3::MakeUintegerChecker<uint16_t> ());
  return tid;
}

WifiReceiverApp::WifiReceiverApp ()
  : m_socket (nullptr),
    m_localPort (0),
    m_packetsReceived (0),
    m_totalDelayUs (0),
    m_minDelayUs (std::numeric_limits<uint64_t>::max ()),
    m_maxDelayUs (0)
{
}

WifiReceiverApp::~WifiReceiverApp ()
{
  m_socket = nullptr; // Ptr will manage memory
}

void
WifiReceiverApp::SetLocalPort (uint16_t port)
{
  m_localPort = port;
}

void
WifiReceiverApp::StartApplication ()
{
  if (m_socket == nullptr)
  {
    m_socket = ns3::Socket::CreateSocket (GetNode (), ns3::UdpSocketFactory::GetTypeId ());
    ns3::InetSocketAddress local = ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), m_localPort);
    if (m_socket->Bind (local) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
  }

  m_socket->SetRecvCallback (ns3::MakeCallback (&WifiReceiverApp::HandleRead, this));
  NS_LOG_INFO ("Receiver: Listening on port " << m_localPort);
}

void
WifiReceiverApp::StopApplication ()
{
  if (m_socket != nullptr)
  {
    m_socket->Close ();
  }

  NS_LOG_INFO ("Receiver: Received " << m_packetsReceived << " packets.");
  if (m_packetsReceived > 0)
  {
    double avgDelayMs = static_cast<double> (m_totalDelayUs) / m_packetsReceived / 1000.0;
    NS_LOG_INFO ("Receiver: Total delay: " << m_totalDelayUs << " us");
    NS_LOG_INFO ("Receiver: Average delay: " << avgDelayMs << " ms");
    NS_LOG_INFO ("Receiver: Min delay: " << m_minDelayUs / 1000.0 << " ms");
    NS_LOG_INFO ("Receiver: Max delay: " << m_maxDelayUs / 1000.0 << " ms");
  }
}

void
WifiReceiverApp::HandleRead (ns3::Ptr<ns3::Socket> socket)
{
  ns3::Ptr<ns3::Packet> packet;
  ns3::Address fromAddress;
  while ((packet = socket->RecvFrom (fromAddress)))
  {
    if (packet->GetSize () > 0)
    {
      TimestampTag tag;
      if (packet->RemovePacketTag (tag))
      {
        uint64_t sendTimeUs = tag.GetTimestamp ();
        uint64_t recvTimeUs = ns3::Simulator::Now ().GetMicroSeconds ();
        uint64_t delayUs = recvTimeUs - sendTimeUs;

        m_packetsReceived++;
        m_totalDelayUs += delayUs;
        m_minDelayUs = std::min (m_minDelayUs, delayUs);
        m_maxDelayUs = std::max (m_maxDelayUs, delayUs);

        ns3::Ipv4Address senderIp = ns3::InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 ();
        NS_LOG_INFO ("Receiver: Packet from " << senderIp << " received at " << ns3::Simulator::Now ()
                                              << ", delay = " << delayUs << " us (packet count: "
                                              << m_packetsReceived << ")");
      }
      else
      {
        NS_LOG_INFO ("Receiver: Received packet without timestamp tag from "
                     << ns3::InetSocketAddress::ConvertFrom (fromAddress).GetIpv4 () << " at "
                     << ns3::Simulator::Now ());
      }
    }
  }
}

int
main (int argc, char *argv[])
{
  ns3::LogComponentEnable ("WifiSenderReceiverExample", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("WifiSenderApp", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("WifiReceiverApp", ns3::LOG_LEVEL_INFO);
  ns3::LogComponentEnable ("UdpSocketImpl", ns3::LOG_LEVEL_INFO);

  uint32_t nWifi = 2;
  std::string phyMode = "DsssRate1Mbps";
  uint32_t packetSize = 1024;
  double interval = 0.1;
  uint32_t maxPackets = 100;
  double simulationTime = 15.0;

  ns3::CommandLine cmd (__FILE__);
  cmd.AddValue ("nWifi", "Number of wifi nodes", nWifi);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "Size of application packets", packetSize);
  cmd.AddValue ("interval", "Interval between packets", interval);
  cmd.AddValue ("maxPackets", "Maximum number of packets to send", maxPackets);
  cmd.AddValue ("simulationTime", "Total duration of the simulation", simulationTime);
  cmd.Parse (argc, argv);

  if (nWifi != 2)
  {
    NS_FATAL_ERROR ("This example is designed for exactly 2 Wi-Fi nodes (sender and receiver).");
  }

  ns3::NodeContainer nodes;
  nodes.Create (nWifi);

  ns3::MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  ns3::WifiHelper wifi;
  wifi.SetStandard (ns3::WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  ns3::YansWifiPhyHelper phy;
  phy.SetPcapDataLinkType (ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);
  ns3::YansWifiChannelHelper channel;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", ns3::DoubleValue (5.180e9));
  phy.SetChannel (channel.Create ());

  ns3::WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  ns3::NetDeviceContainer devices;
  devices = wifi.Install (phy, mac, nodes);

  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  ns3::Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t receiverPort = 9;
  ns3::Ptr<WifiReceiverApp> receiverApp = ns3::CreateObject<WifiReceiverApp> ();
  receiverApp->SetAttribute ("LocalPort", ns3::UintegerValue (receiverPort));
  nodes.Get (1)->AddApplication (receiverApp);
  receiverApp->SetStartTime (ns3::Seconds (0.0));
  receiverApp->SetStopTime (ns3::Seconds (simulationTime));

  ns3::Ptr<WifiSenderApp> senderApp = ns3::CreateObject<WifiSenderApp> ();
  senderApp->SetRemote (interfaces.GetAddress (1), receiverPort);
  senderApp->SetPacketSize (packetSize);
  senderApp->SetInterval (ns3::Seconds (interval));
  senderApp->SetMaxPackets (maxPackets);
  nodes.Get (0)->AddApplication (senderApp);
  senderApp->SetStartTime (ns3::Seconds (1.0));
  senderApp->SetStopTime (ns3::Seconds (simulationTime));

  phy.EnablePcap ("wifi-sender-receiver", devices.Get (0), true);
  phy.EnablePcap ("wifi-sender-receiver", devices.Get (1), true);

  ns3::Simulator::Stop (ns3::Seconds (simulationTime));
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}