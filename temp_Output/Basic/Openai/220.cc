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
  virtual void StartApplication (void)
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        m_socket->Bind (m_local);
        m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
      }
  }

  virtual void StopApplication (void)
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
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom (from)))
      {
        uint32_t pktSize = packet->GetSize ();
        double now = Simulator::Now ().GetSeconds ();
        InetSocketAddress addr = InetSocketAddress::ConvertFrom (from);
        NS_LOG_UNCOND ("Server received packet of size " << pktSize
                       << " from " << addr.GetIpv4 ()
                       << " at " << now << "s");
      }
  }

  Ptr<Socket> m_socket;
  InetSocketAddress m_local;
  uint16_t m_port;
};

class UdpClientApp : public Application
{
public:
  UdpClientApp () : m_socket (0), m_running (false), m_packetSize (1024), m_nPackets (0), m_interval (Seconds (1.0)), m_sent (0) {}
  virtual ~UdpClientApp () { m_socket = 0; }

  void Setup (Address address, uint16_t port, uint32_t packetSize, Time interval)
  {
    m_peer = InetSocketAddress (Ipv4Address::ConvertFrom (address), port);
    m_packetSize = packetSize;
    m_interval = interval;
  }

private:
  virtual void StartApplication (void)
  {
    m_running = true;
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (m_peer);
    SendPacket ();
  }

  virtual void StopApplication (void)
  {
    m_running = false;
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = 0;
      }
  }

  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);
    m_sent++;
    Simulator::Schedule (m_interval, &UdpClientApp::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  Time m_interval;
  uint32_t m_nPackets;
  uint32_t m_sent;
  bool m_running;
};

int main (int argc, char *argv[])
{
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnableAll (LOG_PREFIX_NODE);

  // 1. Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // 2. Set up point-to-point
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  // 3. Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // 4. IP addressing
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 5. Create and install server application
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  uint16_t serverPort = 4000;
  serverApp->Setup (interfaces.GetAddress (1), serverPort);
  nodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // 6. Create and install client application
  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  clientApp->Setup (interfaces.GetAddress (1), serverPort, 512, Seconds (1.0));
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}