#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpBroadcastExample");

class UdpReceiver : public Application
{
public:
  UdpReceiver () {}
  virtual ~UdpReceiver () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication ()
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&UdpReceiver::HandleRead, this));
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
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        uint32_t nodeId = GetNode ()->GetId ();
        NS_LOG_UNCOND ("Node " << nodeId << " received "
                       << packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

class UdpBroadcaster : public Application
{
public:
  UdpBroadcaster () {}
  virtual ~UdpBroadcaster () {}

  void Setup (Ipv4Address broadcastAddr, uint16_t port, uint32_t pktCount, Time pktInterval, uint32_t pktSize)
  {
    m_broadcastAddr = broadcastAddr;
    m_port = port;
    m_pktCount = pktCount;
    m_pktInterval = pktInterval;
    m_pktSize = pktSize;
  }

private:
  virtual void StartApplication ()
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    m_socket->SetAllowBroadcast (true);
    m_sendEvent = Simulator::Schedule (Seconds (0.0), &UdpBroadcaster::SendPacket, this);
    m_sent = 0;
  }

  virtual void StopApplication ()
  {
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
    m_socket->SendTo (packet, 0, InetSocketAddress (m_broadcastAddr, m_port));
    ++m_sent;
    if (m_sent < m_pktCount)
      {
        m_sendEvent = Simulator::Schedule (m_pktInterval, &UdpBroadcaster::SendPacket, this);
      }
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_broadcastAddr;
  uint16_t m_port;
  uint32_t m_pktCount;
  Time m_pktInterval;
  uint32_t m_pktSize;
  uint32_t m_sent;
  EventId m_sendEvent;
};

int main (int argc, char *argv[])
{
  uint32_t nReceivers = 3;
  uint16_t udpPort = 4000;
  uint32_t pktCount = 10;
  Time pktInterval = Seconds (1.0);
  uint32_t pktSize = 64;

  CommandLine cmd;
  cmd.AddValue ("nReceivers", "Number of UDP receiver nodes", nReceivers);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (1 + nReceivers);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("simple-wifi");
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // All nodes static
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Receivers
  for (uint32_t i = 1; i < nodes.GetN (); ++i)
    {
      Ptr<UdpReceiver> recvApp = CreateObject<UdpReceiver> ();
      recvApp->Setup (udpPort);
      nodes.Get (i)->AddApplication (recvApp);
      recvApp->SetStartTime (Seconds (0.0));
      recvApp->SetStopTime (Seconds (pktCount) * pktInterval + Seconds (1.0));
    }

  // Broadcaster
  Ptr<UdpBroadcaster> broadcaster = CreateObject<UdpBroadcaster> ();
  broadcaster->Setup (Ipv4Address ("10.1.1.255"), udpPort, pktCount, pktInterval, pktSize);
  nodes.Get (0)->AddApplication (broadcaster);
  broadcaster->SetStartTime (Seconds (1.0));
  broadcaster->SetStopTime (Seconds (pktCount) * pktInterval + Seconds (2.0));

  Simulator::Stop (Seconds (pktCount) * pktInterval + Seconds (3.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}