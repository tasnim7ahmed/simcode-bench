#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

// Application that acts as a UDP relay
class UdpRelayApp : public Application
{
public:
  UdpRelayApp ();
  virtual ~UdpRelayApp ();

  void Setup (Ipv4Address listenAddress, uint16_t listenPort,
              Ipv4Address forwardAddress, uint16_t forwardPort);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_listenSocket;
  Ptr<Socket> m_forwardSocket;
  Ipv4Address m_listenAddress;
  uint16_t m_listenPort;
  Ipv4Address m_forwardAddress;
  uint16_t m_forwardPort;
};

UdpRelayApp::UdpRelayApp ()
  : m_listenSocket (0),
    m_forwardSocket (0),
    m_listenAddress (),
    m_listenPort (0),
    m_forwardAddress (),
    m_forwardPort (0)
{
}

UdpRelayApp::~UdpRelayApp ()
{
  m_listenSocket = 0;
  m_forwardSocket = 0;
}

void
UdpRelayApp::Setup (Ipv4Address listenAddress, uint16_t listenPort,
                    Ipv4Address forwardAddress, uint16_t forwardPort)
{
  m_listenAddress = listenAddress;
  m_listenPort = listenPort;
  m_forwardAddress = forwardAddress;
  m_forwardPort = forwardPort;
}

void
UdpRelayApp::StartApplication ()
{
  if (!m_listenSocket)
    {
      m_listenSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (m_listenAddress, m_listenPort);
      m_listenSocket->Bind (local);
      m_listenSocket->SetRecvCallback (MakeCallback (&UdpRelayApp::HandleRead, this));
    }
  if (!m_forwardSocket)
    {
      m_forwardSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      // No need to bind; will use SendTo
    }
}

void
UdpRelayApp::StopApplication ()
{
  if (m_listenSocket)
    {
      m_listenSocket->Close ();
      m_listenSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_forwardSocket)
    {
      m_forwardSocket->Close ();
    }
}

void
UdpRelayApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          m_forwardSocket->SendTo (packet, 0, InetSocketAddress (m_forwardAddress, m_forwardPort));
        }
    }
}


class UdpServerLoggerApp : public Application
{
public:
  UdpServerLoggerApp ();
  virtual ~UdpServerLoggerApp ();
  void Setup (Ipv4Address listenAddress, uint16_t listenPort);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  Ipv4Address m_listenAddress;
  uint16_t m_listenPort;
};

UdpServerLoggerApp::UdpServerLoggerApp ()
  : m_socket (0),
    m_listenAddress (),
    m_listenPort (0)
{
}

UdpServerLoggerApp::~UdpServerLoggerApp ()
{
  m_socket = 0;
}

void
UdpServerLoggerApp::Setup (Ipv4Address listenAddress, uint16_t listenPort)
{
  m_listenAddress = listenAddress;
  m_listenPort = listenPort;
}

void
UdpServerLoggerApp::StartApplication ()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (m_listenAddress, m_listenPort);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&UdpServerLoggerApp::HandleRead, this));
    }
}

void
UdpServerLoggerApp::StopApplication ()
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
}

void
UdpServerLoggerApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          uint8_t *buffer = new uint8_t[packet->GetSize () + 1];
          packet->CopyData (buffer, packet->GetSize ());
          buffer[packet->GetSize ()] = '\0';
          NS_LOG_UNCOND ("Server received: " << buffer);
          delete[] buffer;
        }
    }
}

int
main (int argc, char *argv[])
{
  LogComponentEnableAll (LOG_PREFIX_TIME);
  // Uncomment for more verbose logging:
  // LogComponentEnable("UdpServerLoggerApp", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // 0: Client, 1: Relay, 2: Server

  // Link: Client <-> Relay
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d1 = p2p1.Install (nodes.Get (0), nodes.Get (1));

  // Link: Relay <-> Server
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d2 = p2p2.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface1 = address.Assign (d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface2 = address.Assign (d2);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP port assignment
  const uint16_t relayListenPort = 9999;
  const uint16_t serverListenPort = 8000;

  // 1. Install the UDP relay on the relay node (node 1)
  Ptr<UdpRelayApp> relayApp = CreateObject<UdpRelayApp> ();
  relayApp->Setup (
      iface1.GetAddress (1), // Listen address (Relay's iface towards Client)
      relayListenPort,
      iface2.GetAddress (1), // Forward address (Server's iface)
      serverListenPort
      );
  nodes.Get (1)->AddApplication (relayApp);
  relayApp->SetStartTime (Seconds (0.0));
  relayApp->SetStopTime (Seconds (10.0));

  // 2. Install the server on node 2 (server)
  Ptr<UdpServerLoggerApp> serverApp = CreateObject<UdpServerLoggerApp> ();
  serverApp->Setup (
      iface2.GetAddress (1), // Server address
      serverListenPort
      );
  nodes.Get (2)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // 3. Install UDP client (OnOffApplication) on node 0 (client)
  OnOffHelper clientHelper ("ns3::UdpSocketFactory",
                            Address (InetSocketAddress (iface1.GetAddress (1), relayListenPort)));
  clientHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (32));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (2.0)));

  // Use a custom string payload
  clientHelper.SetAttribute ("Remote", AddressValue (InetSocketAddress (iface1.GetAddress (1), relayListenPort)));

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}