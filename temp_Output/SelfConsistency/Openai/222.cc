#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpClientServerExample");

// Custom UDP Server Application
class UdpServerApp : public Application
{
public:
  UdpServerApp ();
  virtual ~UdpServerApp ();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  uint16_t        m_port;
  bool            m_running;
};

UdpServerApp::UdpServerApp ()
  : m_socket (0),
    m_port (0),
    m_running (false)
{
}

UdpServerApp::~UdpServerApp ()
{
  m_socket = 0;
}

void
UdpServerApp::Setup (uint16_t port)
{
  m_port = port;
}

void
UdpServerApp::StartApplication ()
{
  m_running = true;

  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
    }
  m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
}

void
UdpServerApp::StopApplication ()
{
  m_running = false;

  if (m_socket)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket->Close ();
    }
}

void
UdpServerApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      uint32_t rxSize = packet->GetSize ();
      InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
      NS_LOG_INFO ("Server received " << rxSize << " bytes from " << address.GetIpv4 ());
    }
}


// Custom UDP Client Application
class UdpClientApp : public Application
{
public:
  UdpClientApp ();
  virtual ~UdpClientApp ();

  void Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time pktInterval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peerAddress;
  uint16_t        m_peerPort;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_pktInterval;
  uint32_t        m_sent;
  EventId         m_sendEvent;
  bool            m_running;
};

UdpClientApp::UdpClientApp ()
  : m_socket (0),
    m_peerPort (0),
    m_packetSize (0),
    m_nPackets (0),
    m_sent (0),
    m_running (false)
{
}

UdpClientApp::~UdpClientApp ()
{
  m_socket = 0;
}

void
UdpClientApp::Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time pktInterval)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
}

void
UdpClientApp::StartApplication ()
{
  m_running = true;
  m_sent = 0;

  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->Connect (InetSocketAddress (InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 (), m_peerPort));
    }
  SendPacket ();
}

void
UdpClientApp::StopApplication ()
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
UdpClientApp::SendPacket ()
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  ++m_sent;
  if (m_sent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
UdpClientApp::ScheduleTx ()
{
  if (m_running)
    {
      m_sendEvent = Simulator::Schedule (m_pktInterval, &UdpClientApp::SendPacket, this);
    }
}


int
main (int argc, char *argv[])
{
  LogComponentEnable("WifiUdpClientServerExample", LOG_LEVEL_INFO);

  uint32_t nStations = 1;
  uint32_t nPackets = 20;
  uint32_t packetSize = 1024;
  double interval = 0.5; // seconds
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("nPackets", "Number of packets", nPackets);
  cmd.AddValue ("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (nStations); // client
  wifiApNode.Create (1);           // AP+server

  // Configure Wi-Fi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi-network");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterfaces, apInterface;
  staInterfaces = address.Assign (staDevices);
  apInterface = address.Assign (apDevice);

  // Application setup
  uint16_t serverPort = 4000;
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (serverPort);
  wifiApNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (simulationTime));

  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  clientApp->Setup (apInterface.GetAddress (0), serverPort, packetSize, nPackets, Seconds (interval));
  wifiStaNodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0)); // Start a bit later
  clientApp->SetStopTime (Seconds (simulationTime));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}