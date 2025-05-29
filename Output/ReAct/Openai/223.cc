#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiUdpSocketExample");

// Custom UDP server application
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
  virtual void StartApplication () override
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom (from))
      {
        NS_LOG_UNCOND ("Server received " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4 ());
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

// Custom UDP client application
class UdpClientApp : public Application
{
public:
  UdpClientApp () {}
  virtual ~UdpClientApp () {}

  void Setup (Ipv4Address address, uint16_t port, uint32_t packetSize, Time interval)
  {
    m_peerAddress = address;
    m_peerPort = port;
    m_packetSize = packetSize;
    m_interval = interval;
    m_sendEvent = EventId ();
  }

private:
  virtual void StartApplication () override
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress (m_peerAddress, m_peerPort));
    SendPacket ();
  }

  virtual void StopApplication () override
  {
    if (m_sendEvent.IsRunning ())
      {
        Simulator::Cancel (m_sendEvent);
      }
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }
  }

  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);
    m_sendEvent = Simulator::Schedule (m_interval, &UdpClientApp::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  Time m_interval;
  EventId m_sendEvent;
};

int main (int argc, char *argv[])
{
  // Enable logging for demonstration
  LogComponentEnable ("AdhocWifiUdpSocketExample", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure Wi-Fi ad-hoc devices
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Set node positions
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP server on node 1
  uint16_t serverPort = 4000;
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (serverPort);
  nodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // UDP client on node 0
  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  Ipv4Address serverAddr = interfaces.GetAddress (1);
  clientApp->Setup (serverAddr, serverPort, 1024, Seconds (1.0));
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));
  
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}