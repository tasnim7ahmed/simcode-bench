#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpRelayExample");

// Relay Application: receives UDP packets, forwards to server
class UdpRelayApp : public Application
{
public:
  UdpRelayApp () {}
  virtual ~UdpRelayApp () {}

  void Setup (Address listenAddress, Address forwardAddress, uint16_t listenPort, uint16_t forwardPort);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_rxSocket;
  Ptr<Socket> m_txSocket;
  Address m_listenAddress;
  Address m_forwardAddress;
  uint16_t m_listenPort;
  uint16_t m_forwardPort;
};

void
UdpRelayApp::Setup (Address listenAddress, Address forwardAddress, uint16_t listenPort, uint16_t forwardPort)
{
  m_listenAddress = listenAddress;
  m_forwardAddress = forwardAddress;
  m_listenPort = listenPort;
  m_forwardPort = forwardPort;
}

void
UdpRelayApp::StartApplication ()
{
  if (!m_rxSocket)
    {
      m_rxSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::ConvertFrom(m_listenAddress), m_listenPort);
      m_rxSocket->Bind (local);
      m_rxSocket->SetRecvCallback (MakeCallback (&UdpRelayApp::HandleRead, this));
    }
  if (!m_txSocket)
    {
      m_txSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    }
}

void
UdpRelayApp::StopApplication ()
{
  if (m_rxSocket)
    {
      m_rxSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_rxSocket->Close ();
    }
  if (m_txSocket)
    {
      m_txSocket->Close ();
    }
}

void
UdpRelayApp::HandleRead (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      m_txSocket->SendTo (packet, 0, InetSocketAddress (Ipv4Address::ConvertFrom(m_forwardAddress), m_forwardPort));
    }
}

// LogSink: for logging received packets at the server
void LogSink (Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      uint8_t buffer[1024];
      uint32_t size = packet->GetSize ();
      packet->CopyData (buffer, std::min (size, (uint32_t)sizeof(buffer)));
      std::string msg = std::string ((char *)buffer, size);
      NS_LOG_UNCOND ("[" << Simulator::Now ().GetSeconds () << "s] Server received: " << msg);
    }
}

int main (int argc, char *argv[])
{
  // LogComponentEnable ("UdpRelayExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // 0: client, 1: relay, 2: server

  // Point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dev0 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer dev1 = p2p.Install (nodes.Get (1), nodes.Get (2));

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface0 = address.Assign (dev0);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface1 = address.Assign (dev1);

  // UDP ports
  uint16_t relayListenPort = 9000;
  uint16_t serverPort = 9001;

  // 1. CLIENT application: send packet to relay
  uint32_t packetSize = 20;
  double startTime = 1.0;
  double stopTime = 3.0;
  UdpClientHelper clientHelper (iface0.GetAddress (1), relayListenPort);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (1));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (startTime));
  clientApps.Stop (Seconds (stopTime));

  // 2. RELAY application: receive on relayListenPort, forward to serverPort
  Ptr<UdpRelayApp> relayApp = CreateObject<UdpRelayApp> ();
  Address relayListenAddress = iface0.GetAddress (1);
  Address relayForwardAddress = iface1.GetAddress (1);
  relayApp->Setup (relayListenAddress, relayForwardAddress, relayListenPort, serverPort);
  nodes.Get (1)->AddApplication (relayApp);
  relayApp->SetStartTime (Seconds (0.0));
  relayApp->SetStopTime (Seconds (stopTime));

  // 3. SERVER application: receive on serverPort, log received packets
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (iface1.GetAddress (1), serverPort);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&LogSink));

  Simulator::Stop (Seconds (stopTime+1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}