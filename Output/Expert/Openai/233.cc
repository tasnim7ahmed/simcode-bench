#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpPacketReceiver : public Application
{
public:
  UdpPacketReceiver () {}
  virtual ~UdpPacketReceiver () {}
  void Setup (uint16_t port)
  {
    m_port = port;
  }
private:
  virtual void StartApplication (void)
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        m_socket->Bind (local);
        m_socket->SetRecvCallback (MakeCallback (&UdpPacketReceiver::HandleRead, this));
      }
  }
  virtual void StopApplication (void)
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
        if (packet->GetSize () > 0)
          {
            NS_LOG_UNCOND ("At " << Simulator::Now ().GetSeconds ()
                             << "s node " << GetNode ()->GetId ()
                             << " received " << packet->GetSize ()
                             << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4 ());
          }
      }
  }
  Ptr<Socket> m_socket;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  LogComponentEnable ("UdpPacketReceiver", LOG_LEVEL_INFO);
  uint32_t nReceivers = 4;
  uint32_t nNodes = nReceivers + 1;
  uint16_t port = 4000;
  double simulationTime = 5.0;

  NodeContainer nodes;
  nodes.Create (nNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());
  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, NodeContainer (nodes.Get (1), nodes.Get (nNodes - 1)));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, nodes.Get (0));

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (apDevice, staDevices));

  // UDP sender on node 0 (AP)
  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable> ();
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress ("255.255.255.255", port)));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (512));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

  Ptr<Ipv4> ipv4 = nodes.Get (0)->GetObject<Ipv4> ();
  ipv4->SetAttribute ("IpForward", BooleanValue (true));
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<Ipv4> nodeIpv4 = nodes.Get (i)->GetObject<Ipv4> ();
      nodeIpv4->SetAttribute ("IpForward", BooleanValue (true));
      Ipv4InterfaceAddress ifAddr = nodeIpv4->GetAddress (1, 0);
      nodeIpv4->SetBroadcast (ifAddr.GetBroadcast ());
    }

  ApplicationContainer senderApp = onoff.Install (nodes.Get (0));

  // UDP receivers on STA nodes
  for (uint32_t i = 1; i < nNodes; ++i)
    {
      Ptr<UdpPacketReceiver> app = CreateObject<UdpPacketReceiver> ();
      app->Setup (port);
      nodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (0.0));
      app->SetStopTime (Seconds (simulationTime));
    }

  Simulator::Stop (Seconds (simulationTime+0.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}