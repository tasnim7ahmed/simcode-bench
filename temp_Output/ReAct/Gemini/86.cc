#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"
#include "ns3/udp-server.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

class WifiSender : public Application
{
public:
  WifiSender ();
  virtual ~WifiSender();

  void Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t numPackets, Time packetInterval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);

  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  Time m_packetInterval;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

WifiSender::WifiSender ()
  : m_peerPort (0),
    m_packetSize (0),
    m_numPackets (0),
    m_packetsSent (0)
{
}

WifiSender::~WifiSender()
{
  m_socket->Close ();
}

void
WifiSender::Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t numPackets, Time packetInterval)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_packetInterval = packetInterval;
}

void
WifiSender::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  m_socket->Bind ();
  m_socket->Connect (m_peerAddress);

  m_packetsSent = 0;
  SendPacket ();
}

void
WifiSender::StopApplication (void)
{
  CancelEvents ();
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
WifiSender::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_numPackets)
    {
      m_sendEvent = Simulator::Schedule (m_packetInterval, &WifiSender::SendPacket, this);
    }
}

class WifiReceiver : public Application
{
public:
  WifiReceiver ();
  virtual ~WifiReceiver();

  void Setup (uint16_t port);
  void ReceivePacket (Ptr<Socket> socket);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  uint32_t m_packetsReceived;
};

WifiReceiver::WifiReceiver ()
  : m_port (0),
    m_packetsReceived (0)
{
}

WifiReceiver::~WifiReceiver()
{
  m_socket->Close ();
}

void
WifiReceiver::Setup (uint16_t port)
{
  m_port = port;
}

void
WifiReceiver::StartApplication (void)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&WifiReceiver::ReceivePacket, this));
}

void
WifiReceiver::StopApplication (void)
{
  m_socket->Close ();
}

void
WifiReceiver::ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  m_packetsReceived++;
  NS_LOG_INFO ("Received one packet " << packet->GetSize () << " bytes from " << from << " at time " << Simulator::Now ().As (Time::S));
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  uint32_t payloadSize = 1472;
  uint32_t numPackets = 10;
  double interval = 1.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.Parse (argc, argv);

  Time interPacketInterval = Seconds (interval);

  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (wifiPhy, wifiMac, wifiNodes.Get (1));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (wifiPhy, wifiMac, wifiNodes.Get (0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  InternetStackHelper internet;
  internet.Install (wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (staDevices);
  ipv4.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  Address sinkAddress (InetSocketAddress (i.GetAddress (0), port));

  Ptr<WifiSender> appSource = CreateObject<WifiSender> ();
  appSource->Setup (sinkAddress, port, payloadSize, numPackets, interPacketInterval);
  wifiNodes.Get (1)->AddApplication (appSource);
  appSource->SetStartTime (Seconds (1.0));
  appSource->SetStopTime (Seconds (10.0));

  Ptr<WifiReceiver> appSink = CreateObject<WifiReceiver> ();
  appSink->Setup (port);
  wifiNodes.Get (0)->AddApplication (appSink);
  appSink->SetStartTime (Seconds (0.0));
  appSink->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("wifi-simple-ap.tr"));
      wifiPhy.EnablePcapAll ("wifi-simple-ap");
    }

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}