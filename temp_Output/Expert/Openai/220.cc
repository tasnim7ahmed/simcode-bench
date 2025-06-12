#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerApp : public Application
{
public:
  UdpServerApp () : m_socket (0) {}
  virtual ~UdpServerApp () { m_socket = 0; }

  void Setup (Address address, uint16_t port)
  {
    m_local = InetSocketAddress (Ipv4Address::ConvertFrom (address), port);
    m_port = port;
  }

private:
  virtual void StartApplication ()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
    }
  }

  virtual void StopApplication ()
  {
    if (m_socket)
    {
      m_socket->Close ();
      m_socket = 0;
    }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      uint32_t packetSize = packet->GetSize ();
      double t = Simulator::Now ().GetSeconds ();
      Ipv4Address srcAddr = InetSocketAddress::ConvertFrom (from).GetIpv4 ();
      std::cout << "At time " << t << "s server received " << packetSize << " bytes from " << srcAddr << std::endl;
    }
  }

  Ptr<Socket> m_socket;
  InetSocketAddress m_local;
  uint16_t m_port;
};

class UdpClientApp : public Application
{
public:
  UdpClientApp () :
    m_socket (0), m_sendEvent (), m_running (false), m_sent (0),
    m_interval (Seconds (1.0)), m_pktSize (1024), m_nPackets (10) {}
  virtual ~UdpClientApp () { m_socket = 0; }

  void Setup (Address address, uint16_t port, uint32_t nPackets, Time interval, uint32_t pktSize)
  {
    m_peer = InetSocketAddress (Ipv4Address::ConvertFrom (address), port);
    m_nPackets = nPackets;
    m_interval = interval;
    m_pktSize = pktSize;
  }

private:
  virtual void StartApplication ()
  {
    m_running = true;
    m_sent = 0;
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (m_peer);
    SendPacket ();
  }

  virtual void StopApplication ()
  {
    m_running = false;
    if (m_socket)
    {
      m_socket->Close ();
      m_socket = 0;
    }
    Simulator::Cancel (m_sendEvent);
  }

  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_pktSize);
    m_socket->Send (packet);
    ++m_sent;
    if (m_sent < m_nPackets)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &UdpClientApp::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_sent;
  Time m_interval;
  uint32_t m_pktSize;
  uint32_t m_nPackets;
};

int main (int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 20;
  double interval = 0.5; // seconds
  double simTime = 10.0;
  uint16_t serverPort = 4000;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Inter-packet interval (s)", interval);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (interfaces.GetAddress (1), serverPort);
  nodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (simTime));

  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  clientApp->Setup (interfaces.GetAddress (1), serverPort, numPackets, Seconds (interval), packetSize);
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (simTime));

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}