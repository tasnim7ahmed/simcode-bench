#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpCustomAppExample");

class UdpClientApp : public Application
{
public:
  UdpClientApp ();
  virtual ~UdpClientApp ();
  void Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval);
private:
  virtual void StartApplication (void) override;
  virtual void StopApplication (void) override;
  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint16_t        m_peerPort;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_interval;
  uint32_t        m_sent;
  EventId         m_sendEvent;
  bool            m_running;
};

UdpClientApp::UdpClientApp ()
  : m_socket (0),
    m_peer (),
    m_peerPort (0),
    m_packetSize (0),
    m_nPackets (0),
    m_interval (Seconds (1.)),
    m_sent (0),
    m_sendEvent (),
    m_running (false)
{}

UdpClientApp::~UdpClientApp ()
{
  m_socket = 0;
}

void
UdpClientApp::Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_peer = address;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
}

void
UdpClientApp::StartApplication (void)
{
  m_running = true;
  m_sent = 0;
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->Connect (InetSocketAddress (InetSocketAddress::ConvertFrom(m_peer).GetIpv4 (), m_peerPort));
    }
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
  ++m_sent;
  if (m_sent < m_nPackets)
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

class UdpServerApp : public Application
{
public:
  UdpServerApp ();
  virtual ~UdpServerApp ();
  void Setup (uint16_t port);

private:
  virtual void StartApplication (void) override;
  virtual void StopApplication (void) override;
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  uint16_t        m_port;
  Address         m_local;
};

UdpServerApp::UdpServerApp ()
  : m_socket (0),
    m_port (0),
    m_local ()
{}

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
UdpServerApp::StartApplication (void)
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
    }
}

void
UdpServerApp::StopApplication (void)
{
  if (m_socket)
    {
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
      uint32_t recvSize = packet->GetSize ();
      InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
      NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s Server received " << recvSize << " bytes from " << addr.GetIpv4 ());
    }
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("WifiUdpCustomAppExample", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (1); // client

  NodeContainer wifiApNode;
  wifiApNode.Create (1);   // AP

  NodeContainer serverNode;
  serverNode.Create (1);   // server

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Wire up AP and Server via CSMA for infrastructure mode
  CsmaHelper csma;
  NodeContainer backboneNodes;
  backboneNodes.Add (wifiApNode.Get (0));
  backboneNodes.Add (serverNode.Get (0));
  NetDeviceContainer backboneDevices = csma.Install (backboneNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);
  mobility.Install (serverNode);

  // Stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);
  stack.Install (serverNode);

  Ipv4AddressHelper address;

  // Wi-Fi
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  staInterfaces = address.Assign (staDevices);
  apInterface = address.Assign (apDevice);

  // CSMA backbone
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer backboneInterfaces;
  backboneInterfaces = address.Assign (backboneDevices);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Server on server node
  uint16_t udpPort = 4000;
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (udpPort);
  serverNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // UDP Client on STA node
  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  clientApp->Setup (backboneInterfaces.GetAddress (1), udpPort, 1024, 20, Seconds (0.5));
  wifiStaNodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}