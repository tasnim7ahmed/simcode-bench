#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpRelayApp : public Application
{
public:
  UdpRelayApp () {}
  virtual ~UdpRelayApp () {}
  void Setup (Address listenAddress, Address forwardAddress);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  Address m_listenAddress;
  Address m_forwardAddress;
};

void
UdpRelayApp::Setup (Address listenAddress, Address forwardAddress)
{
  m_listenAddress = listenAddress;
  m_forwardAddress = forwardAddress;
}

void
UdpRelayApp::StartApplication ()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->Bind (m_listenAddress);
      m_socket->SetRecvCallback (MakeCallback (&UdpRelayApp::HandleRead, this));
    }
}

void
UdpRelayApp::StopApplication ()
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket = 0;
    }
}

void
UdpRelayApp::HandleRead (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      if (packet->GetSize () > 0)
        {
          Ptr<Socket> forwardSocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
          forwardSocket->Connect (m_forwardAddress);
          forwardSocket->Send (packet);
          forwardSocket->Close ();
        }
    }
}

class UdpLogSink : public Application
{
public:
  UdpLogSink () {}
  virtual ~UdpLogSink () {}
  void Setup (Address listenAddress);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  Address m_listenAddress;
};

void
UdpLogSink::Setup (Address listenAddress)
{
  m_listenAddress = listenAddress;
}

void
UdpLogSink::StartApplication ()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->Bind (m_listenAddress);
      m_socket->SetRecvCallback (MakeCallback (&UdpLogSink::HandleRead, this));
    }
}

void
UdpLogSink::StopApplication ()
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket = 0;
    }
}

void
UdpLogSink::HandleRead (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      if (packet->GetSize () > 0)
        {
          uint8_t *buffer = new uint8_t[packet->GetSize () + 1];
          packet->CopyData (buffer, packet->GetSize ());
          buffer[packet->GetSize ()] = '\0';
          NS_LOG_UNCOND ("Server received message: \"" << buffer << "\" at " << Simulator::Now ().GetSeconds () << "s");
          delete[] buffer;
        }
    }
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpLogSink", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // client, relay, server

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = p2p.Install (nodes.Get (0), nodes.Get (1)); // client <-> relay
  NetDeviceContainer d1d2 = p2p.Install (nodes.Get (1), nodes.Get (2)); // relay <-> server

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  uint16_t relayListenPort = 8000;
  uint16_t serverPort = 9000;

  // Server application
  Ptr<UdpLogSink> serverApp = CreateObject<UdpLogSink> ();
  Address serverListenAddress = InetSocketAddress (i1i2.GetAddress (1), serverPort);
  serverApp->Setup (serverListenAddress);
  nodes.Get (2)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // Relay application
  Ptr<UdpRelayApp> relayApp = CreateObject<UdpRelayApp> ();
  Address relayListenAddress = InetSocketAddress (i0i1.GetAddress (1), relayListenPort);
  Address relayFwdAddress = InetSocketAddress (i1i2.GetAddress (1), serverPort);
  relayApp->Setup (relayListenAddress, relayFwdAddress);
  nodes.Get (1)->AddApplication (relayApp);
  relayApp->SetStartTime (Seconds (0.0));
  relayApp->SetStopTime (Seconds (10.0));

  // Client application
  UdpSocketClientHelper client (i0i1.GetAddress (1), relayListenPort);
  client.SetAttribute ("MaxPackets", UintegerValue (1));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (13));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (2.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}