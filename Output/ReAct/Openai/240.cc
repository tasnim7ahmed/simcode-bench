#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMulticastUdpExample");

class MulticastReceiver : public Application
{
public:
  MulticastReceiver () {}
  virtual ~MulticastReceiver () {}

  void Setup (Address listenAddress, uint16_t port)
  {
    m_listenAddress = listenAddress;
    m_port = port;
  }

private:
  virtual void StartApplication (void)
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::ConvertFrom(m_listenAddress), m_port);
        m_socket->Bind (local);
        m_socket->SetRecvCallback (MakeCallback (&MulticastReceiver::HandleRead, this));
        // Join multicast group
        m_socket->SetIpRecvTos (true);
        m_socket->SetAllowBroadcast (true);
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
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        uint32_t pktSize = packet->GetSize ();
        NS_LOG_UNCOND ("Time " << Simulator::Now ().GetSeconds ()
          << "s: Node " << GetNode ()->GetId ()
          << " received " << pktSize << " bytes from "
          << InetSocketAddress::ConvertFrom(from).GetIpv4 ());
      }
  }

  Ptr<Socket> m_socket;
  Address m_listenAddress;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  uint32_t nReceivers = 3;
  double simulationTime = 10.0; // seconds
  uint16_t multicastPort = 5000;
  Ipv4Address multicastGroup ("224.1.2.3");

  CommandLine cmd;
  cmd.AddValue ("nReceivers", "Number of receiver nodes", nReceivers);
  cmd.Parse (argc, argv);

  NodeContainer wifiNodes;
  wifiNodes.Create (nReceivers + 1);

  NodeContainer senderNode = NodeContainer (wifiNodes.Get (0));
  NodeContainer receiverNodes;
  for (uint32_t i = 0; i < nReceivers; ++i)
    receiverNodes.Add (wifiNodes.Get (i + 1));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("multicast-wifi");

  wifiMac.SetType ("ns3::StaWifiMac",
    "Ssid", SsidValue (ssid),
    "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (wifiPhy, wifiMac, receiverNodes);

  wifiMac.SetType ("ns3::ApWifiMac",
    "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices = wifi.Install (wifiPhy, wifiMac, senderNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));    // Sender node at origin
  for (uint32_t i = 0; i < nReceivers; ++i)
    positionAlloc->Add (Vector (5.0 + i * 5.0, 0.0, 0.0)); // Receivers spaced along x
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  InternetStackHelper internet;
  Ipv4StaticRoutingHelper staticRouting;
  internet.SetRoutingHelper (staticRouting);
  internet.Install (wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  NetDeviceContainer allDevices;
  allDevices.Add (apDevices);
  allDevices.Add (staDevices);
  Ipv4InterfaceContainer interfaces = ipv4.Assign (allDevices);

  // Configure multicast on receivers
  for (uint32_t i = 0; i < nReceivers; ++i)
    {
      Ptr<Node> node = receiverNodes.Get (i);
      Ptr<NetDevice> nd = staDevices.Get (i);
      Ptr<Ipv4> ipv4Node = node->GetObject<Ipv4> ();
      uint32_t iface = ipv4Node->GetInterfaceForDevice (nd);
      ipv4Node->AddMulticastRoute (iface, multicastGroup, iface);
      Ptr<Ipv4StaticRouting> routing = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (node->GetObject<Ipv4> ()->GetRoutingProtocol ());
      if (routing)
        routing->SetDefaultRoute (interfaces.GetAddress (0), iface);
    }

  // Also add static route on AP for multicast forwarding to wireless
  Ptr<Ipv4StaticRouting> apRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (senderNode.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  apRouting->SetDefaultRoute ("10.1.1.1", 1); // AP interface

  // Install multicast receiver application on receiver nodes
  for (uint32_t i = 0; i < nReceivers; ++i)
    {
      Ptr<MulticastReceiver> app = CreateObject<MulticastReceiver> ();
      app->Setup (multicastGroup, multicastPort);
      receiverNodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (0.5));
      app->SetStopTime (Seconds (simulationTime));
    }

  // Create UDP socket on sender
  Ptr<Socket> srcSocket = Socket::CreateSocket (senderNode.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress dst = InetSocketAddress (multicastGroup, multicastPort);
  srcSocket->SetAllowBroadcast (true);
  srcSocket->Connect (dst);

  // Schedule sender to transmit UDP packets
  uint32_t pktSize = 1024;
  uint32_t nPackets = 25;
  Time interPacketInterval = Seconds (0.2);

  for (uint32_t i = 0; i < nPackets; ++i)
    {
      Simulator::Schedule (Seconds (1.0 + i * interPacketInterval.GetSeconds ()),
        &Socket::Send, srcSocket, Create<Packet> (pktSize));
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}