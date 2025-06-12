#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiver");

class SenderApplication : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::SenderApplication")
      .SetParent<Application> ()
      .SetGroupName("Tutorial")
      .AddConstructor<SenderApplication> ()
      .AddAttribute ("PacketSize", "Size of packets sent in bytes",
                     UintegerValue(1024),
                     MakeUintegerAccessor(&SenderApplication::m_packetSize),
                     MakeUintegerChecker<uint32_t>())
      .AddAttribute ("Port", "Destination port",
                     UintegerValue(9),
                     MakeUintegerAccessor(&SenderApplication::m_port),
                     MakeUintegerChecker<uint16_t>())
      .AddAttribute ("Interval", "Interval between packet sends in seconds",
                     TimeValue(Seconds(1.0)),
                     MakeTimeAccessor(&SenderApplication::m_interval),
                     MakeTimeChecker())
      .AddAttribute ("DestinationAddress", "Destination MAC address",
                     MacAddressValue(MacAddress("00:00:00:00:00:01")),
                     MakeMacAddressAccessor(&SenderApplication::m_destAddress),
                     MakeMacAddressChecker())
      .AddAttribute ("Count", "Number of packets to send",
                     UintegerValue(10),
                     MakeUintegerAccessor(&SenderApplication::m_count),
                     MakeUintegerChecker<uint32_t>());
    return tid;
  }

  SenderApplication ();
  virtual ~SenderApplication ();

protected:
  void DoInitialize (void) override;
  void DoDispose (void) override;

private:
  void StartApplication (void) override;
  void StopApplication (void) override;
  void SendPacket (void);

  uint32_t m_packetSize;
  uint16_t m_port;
  Time m_interval;
  MacAddress m_destAddress;
  uint32_t m_count;
  uint32_t m_sent;
  Ptr<Socket> m_socket;
};

SenderApplication::SenderApplication ()
  : m_packetSize(1024),
    m_port(9),
    m_interval(Seconds(1.0)),
    m_count(10),
    m_sent(0),
    m_socket(nullptr)
{
}

SenderApplication::~SenderApplication ()
{
  m_socket = nullptr;
}

void
SenderApplication::DoInitialize (void)
{
  Application::DoInitialize();
}

void
SenderApplication::DoDispose (void)
{
  Application::DoDispose();
}

void
SenderApplication::StartApplication (void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    m_socket->Connect (InetSocketAddress (Ipv4Address::GetBroadcast(), m_port));
    m_socket->SetAllowBroadcast(true);
  }

  Simulator::ScheduleNow (&SenderApplication::SendPacket, this);
}

void
SenderApplication::StopApplication (void)
{
  if (m_socket)
  {
    m_socket->Close ();
    m_socket = nullptr;
  }
}

void
SenderApplication::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send(packet);

  NS_LOG_INFO ("Sent packet at " << Simulator::Now ().GetSeconds () << "s with size " << m_packetSize);

  m_sent++;
  if (m_sent < m_count)
  {
    Simulator::Schedule (m_interval, &SenderApplication::SendPacket, this);
  }
}

class ReceiverApplication : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::ReceiverApplication")
      .SetParent<Application> ()
      .SetGroupName("Tutorial")
      .AddConstructor<ReceiverApplication> ()
      .AddAttribute ("Port", "Listening port",
                     UintegerValue(9),
                     MakeUintegerAccessor(&ReceiverApplication::m_port),
                     MakeUintegerChecker<uint16_t>());
    return tid;
  }

  ReceiverApplication ();
  virtual ~ReceiverApplication ();

protected:
  void DoInitialize (void) override;
  void DoDispose (void) override;

private:
  void StartApplication (void) override;
  void StopApplication (void) override;
  void HandleRead (Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
};

ReceiverApplication::ReceiverApplication ()
  : m_port(9),
    m_socket(nullptr)
{
}

ReceiverApplication::~ReceiverApplication ()
{
  m_socket = nullptr;
}

void
ReceiverApplication::DoInitialize (void)
{
  Application::DoInitialize();
}

void
ReceiverApplication::DoDispose (void)
{
  Application::DoDispose();
}

void
ReceiverApplication::StartApplication (void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback (MakeCallback (&ReceiverApplication::HandleRead, this));
  }
}

void
ReceiverApplication::StopApplication (void)
{
  if (m_socket)
  {
    m_socket->Close ();
    m_socket = nullptr;
  }
}

void
ReceiverApplication::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
  {
    Time recvTime = Simulator::Now ();
    NS_LOG_INFO ("Received packet at " << recvTime.GetSeconds () << "s from " << from);
  }
}

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  ReceiverApplication receiverApp;
  nodes.Get (1)->AddApplication (&receiverApp);
  receiverApp.SetStartTime (Seconds (1.0));
  receiverApp.SetStopTime (Seconds (10.0));

  SenderApplication senderApp;
  nodes.Get (0)->AddApplication (&senderApp);
  senderApp.SetStartTime (Seconds (2.0));
  senderApp.SetStopTime (Seconds (10.0));

  senderApp.m_destAddress = MacAddress::ConvertFrom(interfaces.GetAddress(1));
  senderApp.m_port = 9;
  senderApp.m_interval = Seconds(1.0);
  senderApp.m_count = 5;

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}