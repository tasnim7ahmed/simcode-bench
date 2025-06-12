#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMulticastUdpExample");

class MulticastReceiver : public Application
{
public:
  MulticastReceiver () {}
  virtual ~MulticastReceiver () {}

  void Setup (Address address, uint16_t port)
  {
    m_address = address;
    m_port = port;
  }

private:
  virtual void StartApplication ()
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);

    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);

    m_socket->SetRecvCallback (MakeCallback (&MulticastReceiver::HandleRead, this));
    m_socket->SetAllowBroadcast (true);

    // Join multicast group
    Ptr<Ipv4> ipv4 = GetNode ()->GetObject<Ipv4> ();
    Ipv4Address group = InetSocketAddress::ConvertFrom (m_address).GetIpv4 ();
    ipv4->JoinMulticastGroup (0, group);
  }

  virtual void StopApplication ()
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
        uint32_t pktSize = packet->GetSize ();
        NS_LOG_UNCOND ("Receiver " << GetNode ()->GetId ()
                       << " received " << pktSize << " bytes from "
                       << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
      }
  }

  Ptr<Socket> m_socket;
  Address m_address;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  uint32_t nReceivers = 3;
  double simulationTime = 10.0;
  std::string phyMode = "HtMcs7";

  CommandLine cmd;
  cmd.AddValue ("nReceivers", "Number of receiver nodes", nReceivers);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.Parse (argc, argv);

  uint32_t numNodes = nReceivers + 1; // 1 sender + n receivers

  NodeContainer nodes;
  nodes.Create (numNodes);

  // WiFi PHY/MAC configuration
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("multicast-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer wifiDevices = wifiHelper.Install (phy, mac, nodes);

  // Set mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (5.0),
                                "GridWidth", UintegerValue (4),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (wifiDevices);

  // Multicast details
  Ipv4Address multicastGroup ("225.1.2.3");
  uint16_t multicastPort = 5000;

  // Install Udp server apps (multicast receivers)
  ApplicationContainer serverApps;
  for (uint32_t i = 1; i < numNodes; ++i)
    {
      Ptr<MulticastReceiver> app = CreateObject<MulticastReceiver> ();
      app->Setup (Address (InetSocketAddress (multicastGroup, multicastPort)), multicastPort);
      nodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0));
      app->SetStopTime (Seconds (simulationTime));
      serverApps.Add (app);
    }

  // Multicast sender (node 0)
  Ptr<Socket> sourceSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  sourceSocket->SetAllowBroadcast (true);

  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();

  // Application: Send packets every 0.5s
  Simulator::ScheduleWithContext (sourceSocket->GetNode ()->GetId (), Seconds (2.0),
    [&sourceSocket, multicastGroup, multicastPort, uv, simulationTime] () {
      Time pktInterval = Seconds (0.5);
      uint32_t pktSize = 256;
      double now = Simulator::Now ().GetSeconds ();

      if (now > simulationTime)
        return;

      Ptr<Packet> packet = Create<Packet> (pktSize);
      InetSocketAddress dst = InetSocketAddress (multicastGroup, multicastPort);
      sourceSocket->SendTo (packet, 0, dst);

      Simulator::Schedule (pktInterval, [&sourceSocket, multicastGroup, multicastPort, uv, simulationTime] () {
        Time pktInterval = Seconds (0.5);
        uint32_t pktSize = 256;
        double now = Simulator::Now ().GetSeconds ();
        if (now > simulationTime)
          return;
        Ptr<Packet> packet = Create<Packet> (pktSize);
        InetSocketAddress dst = InetSocketAddress (multicastGroup, multicastPort);
        sourceSocket->SendTo (packet, 0, dst);
        Simulator::Schedule (pktInterval, [&sourceSocket, multicastGroup, multicastPort, uv, simulationTime] () {
           Time pktInterval = Seconds (0.5);
           uint32_t pktSize = 256;
           double now = Simulator::Now ().GetSeconds ();
           if (now > simulationTime)
             return;
           Ptr<Packet> packet = Create<Packet> (pktSize);
           InetSocketAddress dst = InetSocketAddress (multicastGroup, multicastPort);
           sourceSocket->SendTo (packet, 0, dst);
           Simulator::Schedule (pktInterval, [&sourceSocket, multicastGroup, multicastPort, uv, simulationTime] () {
              Time pktInterval = Seconds (0.5);
              uint32_t pktSize = 256;
              double now = Simulator::Now ().GetSeconds ();
              if (now > simulationTime)
                return;
              Ptr<Packet> packet = Create<Packet> (pktSize);
              InetSocketAddress dst = InetSocketAddress (multicastGroup, multicastPort);
              sourceSocket->SendTo (packet, 0, dst);
              Simulator::Schedule (pktInterval, [&sourceSocket, multicastGroup, multicastPort, uv, simulationTime] () {
                // optional: continue or stop scheduling as needed
              });
           });
        });
      });
    });

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}