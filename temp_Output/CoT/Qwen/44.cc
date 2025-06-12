#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleInfra");

class StationToApTraffic : public Application
{
public:
  static TypeId GetTypeId (void);
  StationToApTraffic ();
  virtual ~StationToApTraffic ();

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void SendPacket (void);
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  DataRate m_dataRate;
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetsSent;
};

TypeId
StationToApTraffic::GetTypeId (void)
{
  static TypeId tid = TypeId ("StationToApTraffic")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<StationToApTraffic> ()
    .AddAttribute ("PacketSize", "Size of application packet sent",
                   UintegerValue (1024),
                   MakeUintegerAccessor (&StationToApTraffic::m_packetSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NPackets", "Number of packets to send",
                   UintegerValue (10),
                   MakeUintegerAccessor (&StationToApTraffic::m_nPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("DataRate", "The data rate in on state",
                   DataRateValue (DataRate ("500kb/s")),
                   MakeDataRateAccessor (&StationToApTraffic::m_dataRate),
                   MakeDataRateChecker ());
  return tid;
}

StationToApTraffic::StationToApTraffic ()
  : m_socket (0),
    m_peer (),
    m_packetsSent (0)
{
}

StationToApTraffic::~StationToApTraffic ()
{
  m_socket = nullptr;
}

void
StationToApTraffic::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
  m_socket->Bind (local);

  if (m_peer == Address ())
    {
      TypeId tid = TypeId::LookupByName ("ns3::InetSocketAddress");
      Address address (InetSocketAddress (Ipv4Address ("10.1.1.2"), 9));
      m_peer = address;
    }

  m_socket->Connect (m_peer);

  Simulator::ScheduleNow (&StationToApTraffic::SendPacket, this);
}

void
StationToApTraffic::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket = nullptr;
    }
}

void
StationToApTraffic::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  m_packetsSent++;
  if (m_packetsSent < m_nPackets)
    {
      Time interval = Seconds (static_cast<double> (m_packetSize * 8) / m_dataRate.GetBitRate ());
      Simulator::Schedule (interval, &StationToApTraffic::SendPacket, this);
    }
}

int
main (int argc, char *argv[])
{
  double rss = -80; // dBm
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0; // seconds
  bool verbose = false;
  bool pcap = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("rss", "Received Signal Strength (dBm)", rss);
  cmd.AddValue ("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue ("verbose", "Enable verbose logging output", verbose);
  cmd.AddValue ("pcap", "Enable PCAP tracing", pcap);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiSimpleInfra", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  NetDeviceContainer apDevice;
  NetDeviceContainer staDevice;

  // Configure the channel
  Ptr<YansWifiChannel> wifiChannel = channel.Create ();
  wifiChannel->SetPropagationLossModel (CreateObject<FixedRssLossModel> ()->GetObject<PropagationLossModel> ());
  DynamicCast<FixedRssLossModel> (wifiChannel->GetPropagationLossModel ())->SetRss (rss);
  phy.SetChannel (wifiChannel);

  // Set default power and tx mode
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  // AP Mac configuration
  mac.SetType ("ns3::ApWifiMac");
  apDevice = wifi.Install (phy, mac, nodes.Get (0));

  // Station Mac configuration
  mac.SetType ("ns3::StaWifiMac");
  staDevice = wifi.Install (phy, mac, nodes.Get (1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  // Install server (on AP side)
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Install custom client app (on station)
  StationToApTrafficHelper clientHelper;
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  clientHelper.SetAttribute ("NPackets", UintegerValue (numPackets));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate (static_cast<uint64_t> ((packetSize * 8) / interval))));
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (1));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  if (pcap)
    {
      phy.EnablePcap ("wifi-simple-infra-ap", apDevice.Get (0));
      phy.EnablePcap ("wifi-simple-infra-sta", staDevice.Get (0));
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}