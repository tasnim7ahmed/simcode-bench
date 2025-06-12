#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/icmpv4-l4-protocol.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiBlockAck80211nExample");

class IcmpEchoReplyApp : public Application
{
public:
  IcmpEchoReplyApp () {}
  virtual ~IcmpEchoReplyApp () {}

  void Setup (Ptr<Node> node)
  {
    m_node = node;
    m_socket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
    m_socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), 9));
    m_socket->SetRecvCallback (MakeCallback (&IcmpEchoReplyApp::HandleRead, this));
    m_icmp = m_node->GetObject<Ipv4> ()->GetObject<Icmpv4L4Protocol> ();
  }

private:
  virtual void StartApplication ()
  {
    // Nothing to start; already bound
  }
  virtual void StopApplication ()
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }
  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom (from);

    InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
    Ipv4Address srcAddr = address.GetIpv4 ();

    // ICMP echo reply
    Icmpv4Echo echo;
    echo.SetSequenceNumber (1);
    echo.SetIdentifier (0);
    echo.SetData (packet->CreateFragment (0, packet->GetSize ()));
    Ptr<Packet> icmpPkt = Create<Packet> ();
    icmpPkt->AddHeader (echo);

    Icmpv4Header icmpHdr;
    icmpHdr.SetType (Icmpv4Header::ICMP_ECHO_REPLY);
    icmpHdr.SetCode (0);
    icmpHdr.SetChecksum (0);
    icmpPkt->AddHeader (icmpHdr);

    m_icmp->Send (icmpPkt, m_node->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), srcAddr);
  }

  Ptr<Node> m_node;
  Ptr<Socket> m_socket;
  Ptr<Icmpv4L4Protocol> m_icmp;
};

int main (int argc, char *argv[])
{
  uint32_t numSta = 2;
  uint32_t payloadSize = 512;
  double simulationTime = 10.0;
  std::string dataRate = "20Mbps";
  uint16_t udpPort = 9;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (numSta);
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false),
               "QosSupported", BooleanValue (true));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "QosSupported", BooleanValue (true));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Set Block Ack parameters (wifi standard 802.11n)
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_BlockAckThreshold", UintegerValue (4));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BlockAckInactivityTimeout", UintegerValue (200)); // ms

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (10.0),
                                "GridWidth", UintegerValue (numSta+1),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpClientHelper client (apInterface.GetAddress (0), udpPort);
  client.SetAttribute ("MaxPackets", UintegerValue (10000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer clientApp = client.Install (wifiStaNodes.Get (0)); // First STA only is QoS station
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simulationTime - 1));

  // Setup a server at AP which replies with ICMP echo reply upon UDP packet
  Ptr<IcmpEchoReplyApp> icmpApp = CreateObject<IcmpEchoReplyApp> ();
  icmpApp->Setup (wifiApNode.Get (0));
  wifiApNode.Get (0)->AddApplication (icmpApp);
  icmpApp->SetStartTime (Seconds (0.5));
  icmpApp->SetStopTime (Seconds (simulationTime));

  // Enable packet capture
  phy.EnablePcap ("block-ack-ap", apDevice.Get (0), true);

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}