#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpClientApp : public Application
{
public:
  UdpClientApp ();
  virtual ~UdpClientApp();
  void Setup (std::vector<Ipv4Address> serverAddresses, std::vector<uint16_t> serverPorts, uint32_t packetSize, uint32_t nPackets, Time interval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket> m_socket;
  std::vector<Ipv4Address> m_peerAddresses;
  std::vector<uint16_t> m_peerPorts;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  uint32_t m_sent;
  Time m_interval;
  EventId m_sendEvent;
  bool m_running;
};

UdpClientApp::UdpClientApp ()
  : m_socket (0),
    m_packetSize (0),
    m_nPackets (0),
    m_sent (0),
    m_running (false)
{
}

UdpClientApp::~UdpClientApp()
{
  m_socket = 0;
}

void
UdpClientApp::Setup (std::vector<Ipv4Address> serverAddresses, std::vector<uint16_t> serverPorts, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_peerAddresses = serverAddresses;
  m_peerPorts = serverPorts;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
}

void
UdpClientApp::StartApplication (void)
{
  m_running = true;
  m_sent = 0;
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  SendPacket ();
}

void
UdpClientApp::StopApplication (void)
{
  m_running = false;
  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);
  if (m_socket)
    m_socket->Close ();
}

void
UdpClientApp::SendPacket (void)
{
  for (size_t i = 0; i < m_peerAddresses.size (); ++i)
    {
      Ptr<Packet> packet = Create<Packet> (m_packetSize);
      m_socket->SendTo (packet, 0, InetSocketAddress (m_peerAddresses[i], m_peerPorts[i]));
    }
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
  virtual ~UdpServerApp();
  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

UdpServerApp::UdpServerApp ()
  : m_socket (0),
    m_port (0)
{
}

UdpServerApp::~UdpServerApp()
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
      if (packet->GetSize () > 0)
        {
          InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
          NS_LOG_UNCOND ("Time " << Simulator::Now ().GetSeconds ()
                         << "s | Server (port " << m_port << ") received " << packet->GetSize ()
                         << " bytes from " << address.GetIpv4 ());
        }
    }
}

int
main (int argc, char *argv[])
{
  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_FUNC);

  // Nodes: 0 = client, 1 = server1, 2 = server2
  NodeContainer nodes;
  nodes.Create (3);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t server1Port = 9001;
  uint16_t server2Port = 9002;

  // Server 1 app on node 1
  Ptr<UdpServerApp> server1App = CreateObject<UdpServerApp> ();
  server1App->Setup (server1Port);
  nodes.Get (1)->AddApplication (server1App);
  server1App->SetStartTime (Seconds (0.0));
  server1App->SetStopTime (Seconds (10.0));

  // Server 2 app on node 2
  Ptr<UdpServerApp> server2App = CreateObject<UdpServerApp> ();
  server2App->Setup (server2Port);
  nodes.Get (2)->AddApplication (server2App);
  server2App->SetStartTime (Seconds (0.0));
  server2App->SetStopTime (Seconds (10.0));

  // Client app on node 0
  std::vector<Ipv4Address> serverAddresses = {interfaces.GetAddress(1), interfaces.GetAddress(2)};
  std::vector<uint16_t> serverPorts = {server1Port, server2Port};
  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  uint32_t packetSize = 1024;
  uint32_t nPackets = 20;
  Time sendInterval = Seconds (0.5);
  clientApp->Setup (serverAddresses, serverPorts, packetSize, nPackets, sendInterval);
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}