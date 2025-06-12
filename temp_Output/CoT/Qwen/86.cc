#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomWifiApp");

class CustomSender : public Application
{
public:
  static TypeId GetTypeId (void);
  CustomSender ();
  virtual ~CustomSender ();

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void SendPacket (void);
  uint32_t m_packetsSent;
  uint32_t m_totalPackets;
  uint32_t m_packetSize;
  Address m_destAddress;
  uint16_t m_destPort;
  Time m_interval;
  Ptr<Socket> m_socket;
};

class CustomReceiver : public Application
{
public:
  static TypeId GetTypeId (void);
  CustomReceiver ();
  virtual ~CustomReceiver ();

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void HandleRead (Ptr<Socket> socket);
  uint16_t m_port;
  Ptr<Socket> m_socket;
};

TypeId
CustomSender::GetTypeId (void)
{
  static TypeId tid = TypeId ("CustomSender")
    .SetParent<Application> ()
    .AddConstructor<CustomSender> ()
    .AddAttribute ("PacketSize", "Size of packets sent in bytes.",
                   UintegerValue(512),
                   MakeUintegerAccessor(&CustomSender::m_packetSize),
                   MakeUintegerChecker<uint32_t>())
    .AddAttribute ("TotalPackets", "Total number of packets to send.",
                   UintegerValue(10),
                   MakeUintegerAccessor(&CustomSender::m_totalPackets),
                   MakeUintegerChecker<uint32_t>())
    .AddAttribute ("Interval", "Time interval between packet sends.",
                   TimeValue(Seconds(1.0)),
                   MakeTimeAccessor(&CustomSender::m_interval),
                   MakeTimeChecker())
    .AddAttribute ("DestinationAddress", "Destination address for sending packets.",
                   AddressValue(),
                   MakeAddressAccessor(&CustomSender::m_destAddress),
                   MakeAddressChecker())
    .AddAttribute ("DestinationPort", "Destination port for sending packets.",
                   UintegerValue(9),
                   MakeUintegerAccessor(&CustomSender::m_destPort),
                   MakeUintegerChecker<uint16_t>());
  return tid;
}

CustomSender::CustomSender ()
  : m_packetsSent(0), m_totalPackets(0), m_packetSize(0), m_destPort(0), m_socket(nullptr)
{
}

CustomSender::~CustomSender ()
{
  m_socket = nullptr;
}

void
CustomSender::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_destAddress), m_destPort));
  SendPacket ();
}

void
CustomSender::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
CustomSender::SendPacket (void)
{
  if (m_packetsSent >= m_totalPackets)
    {
      return;
    }

  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  NS_LOG_INFO ("Sent packet " << m_packetsSent + 1 << " at " << Now ().As (Time::S));
  m_packetsSent++;
  Simulator::Schedule (m_interval, &CustomSender::SendPacket, this);
}

TypeId
CustomReceiver::GetTypeId (void)
{
  static TypeId tid = TypeId ("CustomReceiver")
    .SetParent<Application> ()
    .AddConstructor<CustomReceiver> ()
    .AddAttribute ("Port", "Port on which the application listens.",
                   UintegerValue(9),
                   MakeUintegerAccessor(&CustomReceiver::m_port),
                   MakeUintegerChecker<uint16_t>());
  return tid;
}

CustomReceiver::CustomReceiver ()
  : m_port(0), m_socket(nullptr)
{
}

CustomReceiver::~CustomReceiver ()
{
  m_socket = nullptr;
}

void
CustomReceiver::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&CustomReceiver::HandleRead, this));
}

void
CustomReceiver::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
CustomReceiver::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      static uint32_t receivedCount = 0;
      receivedCount++;
      Time rxTime = Now ();
      NS_LOG_INFO ("Received packet " << receivedCount << " at " << rxTime.As (Time::S));
    }
}

int
main (int argc, char *argv[])
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
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  mac.SetType ("ns3::AdhocWifiMac");
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

  CustomReceiverHelper receiverHelper;
  receiverHelper.SetAttribute ("Port", UintegerValue (9));
  ApplicationContainer receiverApp = receiverHelper.Install (nodes.Get (1));
  receiverApp.Start (Seconds (1.0));
  receiverApp.Stop (Seconds (10.0));

  CustomSenderHelper senderHelper;
  senderHelper.SetAttribute ("PacketSize", UintegerValue (512));
  senderHelper.SetAttribute ("TotalPackets", UintegerValue (5));
  senderHelper.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
  senderHelper.SetAttribute ("DestinationAddress", AddressValue (interfaces.GetAddress (1)));
  senderHelper.SetAttribute ("DestinationPort", UintegerValue (9));
  ApplicationContainer senderApp = senderHelper.Install (nodes.Get (0));
  senderApp.Start (Seconds (2.0));
  senderApp.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}