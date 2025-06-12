#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CustomUdpSocketExample");

// Server application
class UdpServerApp : public Application
{
public:
  UdpServerApp () {}
  virtual ~UdpServerApp () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
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
    while (Ptr<Packet> packet = socket->Recv())
      {
        Address from;
        socket->GetSockName (from);
        NS_LOG_UNCOND ("At " << Simulator::Now ().GetSeconds ()
                       << "s server received " << packet->GetSize ()
                       << " bytes from "
                       << InetSocketAddress::ConvertFrom (socket->GetPeerName ()).GetIpv4 ());
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

// Client application
class UdpClientApp : public Application
{
public:
  UdpClientApp ()
    : m_socket (0),
      m_sendEvent (),
      m_running (false),
      m_pktSize (1024),
      m_nPackets (0),
      m_sent (0),
      m_interval (Seconds (1.0))
  {
  }
  virtual ~UdpClientApp () { m_socket = 0; }

  void Setup (Address address, uint32_t pktSize, uint32_t nPackets, Time interval)
  {
    m_peer = address;
    m_pktSize = pktSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

private:
  virtual void StartApplication (void)
  {
    m_running = true;
    m_sent = 0;
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (m_peer);
    SendPacket ();
  }

  virtual void StopApplication (void)
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
  uint32_t m_pktSize;
  uint32_t m_nPackets;
  uint32_t m_sent;
  Time m_interval;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("CustomUdpSocketExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 4000;
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (port);
  nodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  Address serverAddr = InetSocketAddress (interfaces.GetAddress (1), port);
  double interval = 1.0; // seconds
  uint32_t numPackets = 10;
  uint32_t pktSize = 512;
  clientApp->Setup (serverAddr, pktSize, numPackets, Seconds (interval));
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}