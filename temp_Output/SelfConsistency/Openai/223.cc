#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"

using namespace ns3;

// Custom UDP client application
class UdpClientApp : public Application
{
public:
  UdpClientApp ();
  virtual ~UdpClientApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  uint32_t        m_packetsSent;
  Time            m_interval;
  EventId         m_sendEvent;
  bool            m_running;
};

UdpClientApp::UdpClientApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_packetsSent (0),
    m_interval (Seconds (1.0)),
    m_sendEvent (),
    m_running (false)
{
}

UdpClientApp::~UdpClientApp()
{
  m_socket = 0;
}

void
UdpClientApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
}

void
UdpClientApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
UdpClientApp::StopApplication (void)
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
UdpClientApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  ++m_packetsSent;
  if (m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
UdpClientApp::ScheduleTx (void)
{
  if (m_running)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &UdpClientApp::SendPacket, this);
    }
}

// Custom UDP server application
class UdpServerApp : public Application
{
public:
  UdpServerApp ();
  virtual ~UdpServerApp();

  void Setup (Ptr<Socket> socket, Address address);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  Address         m_local;
};

UdpServerApp::UdpServerApp ()
  : m_socket (0),
    m_local ()
{
}

UdpServerApp::~UdpServerApp()
{
  m_socket = 0;
}

void
UdpServerApp::Setup (Ptr<Socket> socket, Address address)
{
  m_socket = socket;
  m_local = address;
}

void
UdpServerApp::StartApplication (void)
{
  if (!m_socket)
    {
      NS_LOG_ERROR ("No socket provided to UdpServerApp");
      return;
    }
  if (InetSocketAddress::IsMatchingType (m_local))
    {
      m_socket->Bind (m_local);
    }
  else
    {
      m_socket->Bind ();
    }
  m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
}

void
UdpServerApp::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
UdpServerApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_UNCOND ("At time " << Simulator::Now ().GetSeconds ()
                       << "s server received " << packet->GetSize ()
                       << " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
    }
}

int
main (int argc, char *argv[])
{
  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnable("UdpServerApp", LOG_LEVEL_INFO);

  // Create two nodes (client and server)
  NodeContainer nodes;
  nodes.Create (2);

  // Set up Wi-Fi in ad-hoc mode
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
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

  // Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Create the server socket and application
  uint16_t servPort = 4000;
  Address servAddress (InetSocketAddress (interfaces.GetAddress (1), servPort));
  Ptr<Socket> servSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (servSocket, servAddress);
  nodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // Create the client socket and application
  uint32_t packetSize = 1024; // bytes
  uint32_t numPackets = 20;
  Time interPacketInterval = Seconds (0.5);

  Ptr<Socket> cliSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  clientApp->Setup (cliSocket, servAddress, packetSize, numPackets, interPacketInterval);
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}