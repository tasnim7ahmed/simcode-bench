#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpCustomSocketExample");

class UdpServerApp : public Application
{
public:
  UdpServerApp () {}
  virtual ~UdpServerApp () {}

  void Setup (Address address)
  {
    m_local = address;
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
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
        m_socket->Close ();
      }
  }
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        NS_LOG_INFO ("Server received " << packet->GetSize ()
                     << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4 ());
      }
  }
  Ptr<Socket> m_socket;
  Address m_local;
};

class UdpClientApp : public Application
{
public:
  UdpClientApp () {}
  virtual ~UdpClientApp () {}

  void Setup (Address peer, uint32_t packetSize, uint32_t nPackets, Time interval)
  {
    m_peer = peer;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
    m_sent = 0;
  }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_sendEvent = Simulator::Schedule (Seconds (0.0), &UdpClientApp::SendPacket, this);
  }
  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
    Simulator::Cancel (m_sendEvent);
  }
  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->SendTo (packet, 0, m_peer);
    ++m_sent;
    if (m_sent < m_nPackets)
      {
        m_sendEvent = Simulator::Schedule (m_interval, &UdpClientApp::SendPacket, this);
      }
  }
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
  EventId m_sendEvent;
};

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);
  NodeContainer serverNode;
  serverNode.Create (1);

  // Configure Wi-Fi (Infrastructure mode)
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  WifiMacHelper mac;

  Ssid ssid = Ssid ("ns3-wifi-udp");

  // STA (Client)
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNodes);

  // AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Connect AP <-> Server by CSMA (wired)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NodeContainer apServer;
  apServer.Add (wifiApNode.Get (0));
  apServer.Add (serverNode.Get (0));
  NetDeviceContainer csmaDevices = csma.Install (apServer);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));   // STA
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));   // AP
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));  // Server
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);
  mobility.Install (serverNode);

  // Internet and IP addresses
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);
  stack.Install (serverNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevice);
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevice);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

  // Static routing; AP will route between Wi-Fi LAN and server via CSMA
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> apStaticRouting = ipv4RoutingHelper.GetStaticRouting (wifiApNode.Get (0)->GetObject<Ipv4> ());
  apStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"), 2);

  Ptr<Ipv4StaticRouting> serverStaticRouting = ipv4RoutingHelper.GetStaticRouting (serverNode.Get (0)->GetObject<Ipv4> ());
  serverStaticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), 1);

  // UDP server on serverNode, listening on 4000
  uint16_t port = 4000;
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  InetSocketAddress serverAddr = InetSocketAddress (csmaInterfaces.GetAddress (1), port);
  serverApp->Setup (serverAddr);
  serverNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // UDP client on wifiStaNode, sends to server at 0.5s interval, total 20 packets of 1024 bytes each
  Ptr<UdpClientApp> clientApp = CreateObject<UdpClientApp> ();
  InetSocketAddress remoteAddr = InetSocketAddress (csmaInterfaces.GetAddress (1), port);
  clientApp->Setup (remoteAddr, 1024, 20, Seconds (0.5));
  wifiStaNodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  LogComponentEnable ("WifiUdpCustomSocketExample", LOG_LEVEL_INFO);

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}