#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/icmpv4.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiBlockAck80211nExample");

class IcmpReplyApp : public Application
{
public:
  IcmpReplyApp ();
  virtual ~IcmpReplyApp();

  void Setup (Ptr<Node> node);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void RxCallback (Ptr<Socket> socket);

  Ptr<Node> m_node;
  Ptr<Socket> m_socket;
};

IcmpReplyApp::IcmpReplyApp ()
  : m_node (0),
    m_socket (0)
{
}

IcmpReplyApp::~IcmpReplyApp ()
{
  m_socket = 0;
}

void IcmpReplyApp::Setup (Ptr<Node> node)
{
  m_node = node;
}

void IcmpReplyApp::StartApplication (void)
{
  m_socket = Socket::CreateSocket (m_node, UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&IcmpReplyApp::RxCallback, this));
}

void IcmpReplyApp::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket = 0;
    }
}

void IcmpReplyApp::RxCallback (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
      Ipv4Address peerAddr = address.GetIpv4 ();

      // Compose ICMP echo reply
      Ptr<Packet> icmpPacket = Create<Packet> (packet->GetSize ());
      Ipv4Header ipv4Header;
      Icmpv4Echo icmp (1, 0); // Type 1: Echo reply, code 0

      Icmpv4Header icmpHeader;
      icmpHeader.SetType (Icmpv4Header::ICMPV4_ECHO_REPLY);
      icmpHeader.SetCode (0);

      icmpPacket->AddHeader (icmpHeader);

      // Use raw socket to send ICMP back
      Ptr<Socket> icmpSocket = Socket::CreateSocket (m_node, Ipv4RawSocketFactory::GetTypeId ());
      icmpSocket->SetAttribute ("Protocol", UintegerValue (1)); // ICMP
      icmpSocket->Connect (InetSocketAddress (peerAddr, 0));
      icmpSocket->Send (icmpPacket);
      icmpSocket->Close ();
    }
}

int main (int argc, char *argv[])
{
  uint32_t nSta = 2;
  double simTime = 10.0;
  std::string dataRate = "54Mbps";
  std::string udpAppDataRate = "10Mbps";
  uint32_t appPacketSize = 1024;
  uint32_t blockAckThreshold = 4;
  Time blockAckTimeout = MilliSeconds (32);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (nSta);
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n-bockack");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false),
               "QosSupported", BooleanValue (true));
  NetDeviceContainer staDevs = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "QosSupported", BooleanValue (true));
  NetDeviceContainer apDevs = wifi.Install (phy, mac, wifiApNode);

  // Enable Block Ack and set its parameters
  for (uint32_t i = 0; i < staDevs.GetN (); ++i)
    {
      Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice> (staDevs.Get (i));
      Ptr<EdcaTxopN> edca = dev->GetMac ()->GetEdcaTxopN (AC_VO);
      edca->SetBlockAckThreshold (blockAckThreshold);
      edca->SetBlockAckInactivityTimeout (blockAckTimeout);
    }
  Ptr<WifiNetDevice> apDev = DynamicCast<WifiNetDevice> (apDevs.Get (0));
  Ptr<ApWifiMac> apMac = DynamicCast<ApWifiMac> (apDev->GetMac ());
  for (uint32_t ac = 0; ac < 4; ++ac)
    {
      Ptr<EdcaTxopN> edca = apMac->GetEdcaTxopN ((AcIndex)ac);
      edca->SetBlockAckThreshold (blockAckThreshold);
      edca->SetBlockAckInactivityTimeout (blockAckTimeout);
    }

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Place nodes
  Ptr<MobilityModel> mob;
  mob = wifiStaNodes.Get (0)->GetObject<MobilityModel> ();
  mob->SetPosition (Vector (0.0, 0.0, 0.0));
  mob = wifiStaNodes.Get (1)->GetObject<MobilityModel> ();
  mob->SetPosition (Vector (5.0, 0.0, 0.0));
  mob = wifiApNode.Get (0)->GetObject<MobilityModel> ();
  mob->SetPosition (Vector (2.5, 3.0, 0.0));

  InternetStackHelper internet;
  internet.Install (wifiStaNodes);
  internet.Install (wifiApNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIfs, apIf;
  staIfs = ipv4.Assign (staDevs);
  apIf = ipv4.Assign (apDevs);

  Ipv4StaticRoutingHelper ipv4Routing;
  for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
      Ptr<Ipv4StaticRouting> staticRouting = ipv4Routing.GetStaticRouting (wifiStaNodes.Get (i)->GetObject<Ipv4> ());
      staticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), 1);
    }
  Ptr<Ipv4StaticRouting> staticRoutingAp = ipv4Routing.GetStaticRouting (wifiApNode.Get (0)->GetObject<Ipv4> ());
  staticRoutingAp->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"), 1);

  // Install UDP Client: STA 0 -> AP
  uint16_t port = 9;
  UdpClientHelper udpClient (apIf.GetAddress (0), port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (10000));
  udpClient.SetAttribute ("Interval", TimeValue (MicroSeconds (200)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (appPacketSize));
  udpClient.SetAttribute ("DataRate", DataRateValue (DataRate (udpAppDataRate)));

  ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simTime - 1.0));

  // Install ICMP "server" app on AP
  Ptr<IcmpReplyApp> icmpApp = CreateObject<IcmpReplyApp> ();
  icmpApp->Setup (wifiApNode.Get (0));
  wifiApNode.Get (0)->AddApplication (icmpApp);
  icmpApp->SetStartTime (Seconds (0.9));
  icmpApp->SetStopTime (Seconds (simTime));

  // Enable pcap at the AP
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-block-ack-ap", apDevs.Get (0), true, true);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}