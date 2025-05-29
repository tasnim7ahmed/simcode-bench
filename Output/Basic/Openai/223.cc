#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiUdpSimulation");

// Custom UDP Server Application
class UdpServerApp : public Application
{
public:
  UdpServerApp () {}
  virtual ~UdpServerApp () {}
  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  uint16_t        m_port;
};

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
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }
}

void
UdpServerApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_UNCOND ("[Server] Received packet of size " << packet->GetSize () <<
                     " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
    }
}

// Custom UDP Client Application
class UdpClientApp : public Application
{
public:
  UdpClientApp () {}
  virtual ~UdpClientApp () {}
  void Setup (Address address, uint32_t packetSize, uint32_t nPackets, Time interval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_interval;
  uint32_t        m_sent;
  EventId         m_sendEvent;
};

void
UdpClientApp::Setup (Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
  m_sent = 0;
}

void
UdpClientApp::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket->Connect (m_peer);
  m_sent = 0;
  SendPacket ();
}

void
UdpClientApp::StopApplication (void)
{
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
  m_sent++;
  if (m_sent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
UdpClientApp::ScheduleTx (void)
{
  m_sendEvent = Simulator::Schedule (m_interval, &UdpClientApp::SendPacket, this);
}

int 
main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 20;
  double interval = 0.5;
  uint16_t serverPort = 4000;
  double simTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue ("serverPort", "UDP server port", serverPort);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (serverPort);
  nodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (simTime));

  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  Address serverAddr = InetSocketAddress (interfaces.GetAddress (1), serverPort);
  clientApp->Setup (serverAddr, packetSize, numPackets, Seconds (interval));
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (simTime));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}