#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class MulticastReceiver : public Application
{
public:
  MulticastReceiver () {}
  virtual ~MulticastReceiver () {}
  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

void
MulticastReceiver::Setup (uint16_t port)
{
  m_port = port;
}

void
MulticastReceiver::StartApplication ()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      if (m_socket->Bind (local) == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      m_socket->SetRecvCallback (MakeCallback (&MulticastReceiver::HandleRead, this));
    }
}

void
MulticastReceiver::StopApplication ()
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket = 0;
    }
}

void
MulticastReceiver::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_UNCOND ("Node " << GetNode ()->GetId ()
                     << " received " << packet->GetSize ()
                     << " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
    }
}

int
main (int argc, char *argv[])
{
  uint32_t nReceivers = 4;
  double simulationTime = 5.0;
  uint16_t port = 5000;
  std::string phyMode ("HtMcs7");
  Ipv4Address multicastGroup ("224.1.2.3");

  CommandLine cmd;
  cmd.AddValue ("nReceivers", "Number of multicast receivers", nReceivers);
  cmd.Parse (argc, argv);

  uint32_t nNodes = nReceivers + 1;

  NodeContainer nodes;
  nodes.Create (nNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (Ssid ("multicast-wifi")),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevs = wifi.Install (wifiPhy, wifiMac, NodeContainer (nodes.Get (1), nodes.Get (nNodes-1)));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (Ssid ("multicast-wifi")));

  NetDeviceContainer apDev = wifi.Install (wifiPhy, wifiMac, nodes.Get (0));

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (NetDeviceContainer (apDev, staDevs));

  for (uint32_t i = 1; i < nNodes; ++i)
    {
      Ptr<Ipv4StaticRouting> multicastRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting> (nodes.Get (i)->GetObject<Ipv4> ()->GetRoutingProtocol ());
      multicastRouting->AddMulticastRoute (
        interfaces.Get (i),
        multicastGroup,
        interfaces.Get (0),
        1
      );
    }

  Ptr<Ipv4> senderIpv4 = nodes.Get (0)->GetObject<Ipv4> ();
  senderIpv4->GetRoutingProtocol ()->NotifyAddAddress (0, Ipv4InterfaceAddress (multicastGroup, Ipv4Mask ("255.255.255.255")));

  // Receivers join group
  for (uint32_t i = 1; i < nNodes; ++i)
    {
      Ptr<Ipv4StaticRouting> routing = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting> (nodes.Get (i)->GetObject<Ipv4> ()->GetRoutingProtocol ());
      routing->JoinMulticastGroup (interfaces.Get (i), multicastGroup);
    }

  // Install multicast receivers
  for (uint32_t i = 1; i < nNodes; ++i)
    {
      Ptr<MulticastReceiver> app = CreateObject<MulticastReceiver> ();
      app->Setup (port);
      nodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (0.));
      app->SetStopTime (Seconds (simulationTime));
    }

  // Sender application
  uint32_t packetSize = 1024;
  uint32_t nPackets = 10;
  double pktInterval = 0.5;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (multicastGroup, port));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mb/s")));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer senderApps = onoff.Install (nodes.Get (0));
  senderApps.Start (Seconds (1.0));
  senderApps.Stop (Seconds (simulationTime));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}