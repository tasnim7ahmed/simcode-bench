#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpWiFiSimulation");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("UdpServer")
      .SetParent<Application> ()
      .AddConstructor<UdpServer> ();
    return tid;
  }

  UdpServer () {}
  virtual ~UdpServer () {}

private:
  virtual void StartApplication (void)
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));
    NS_LOG_INFO ("Server socket created and bound.");
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
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        NS_LOG_INFO ("Received packet of size " << packet->GetSize () << " from " << from);
      }
  }

  Ptr<Socket> m_socket;
};

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Create nodes: 1 server + 3 clients
  NodeContainer nodes;
  nodes.Create (4);
  Ptr<Node> serverNode = nodes.Get (0);
  NodeContainer clientNodes = NodeContainer (nodes, 1, 3); // indices 1,2,3

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Mobility: static positions
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0),
                                 "GridWidth", UintegerValue (4),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install server application
  UdpServerHelper server;
  ApplicationContainer serverApp = server.Install (serverNode);
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Install client applications
  uint16_t port = 9;
  for (uint32_t i = 0; i < clientNodes.GetN (); ++i)
    {
      OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (0), port));
      onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("1kbps")));
      onoff.SetAttribute ("PacketSize", UintegerValue (512));

      ApplicationContainer clientApp = onoff.Install (clientNodes.Get (i));
      clientApp.Start (Seconds (2.0 + i));  // staggered start times
      clientApp.Stop (Seconds (10.0));
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}