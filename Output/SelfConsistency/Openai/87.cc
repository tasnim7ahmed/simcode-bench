/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/data-collector.h"
#include "ns3/packet.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDistanceExperiment");

// Global statistics variables
uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;
double g_totalDelay = 0.0;

// Packet Transmission Callback
void TxCallback (Ptr<const Packet> packet)
{
  ++g_txPackets;
}

// Packet Reception Callback
void RxCallback (Ptr<const Packet> packet, const Address &address)
{
  ++g_rxPackets;
  // Try to extract the packet timestamp for delay calculation
  Time sent, received = Simulator::Now ();
  if (packet->PeekPacketTag<PacketTagTime> ())
    {
      PacketTagTime tag;
      packet->PeekPacketTag (tag);
      sent = tag.GetTime ();
      double delay = (received - sent).GetSeconds ();
      g_totalDelay += delay;
    }
}

// Tag to store packet send time
class PacketTagTime : public Tag
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("PacketTagTime")
      .SetParent<Tag> ()
      .AddConstructor<PacketTagTime> ();
    return tid;
  }
  virtual TypeId GetInstanceTypeId (void) const { return GetTypeId (); }
  virtual uint32_t GetSerializedSize (void) const { return 8; }
  virtual void Serialize (TagBuffer i) const { i.WriteDouble (m_time.GetSeconds ()); }
  virtual void Deserialize (TagBuffer i) { m_time = Seconds (i.ReadDouble ()); }
  virtual void Print (std::ostream &os) const { os << "t=" << m_time.GetSeconds (); }

  void SetTime (Time t) { m_time = t; }
  Time GetTime () const { return m_time; }
private:
  Time m_time;
};

// Modified OnOffApplication to embed a timestamp tag in each packet
class TimestampOnOffApplication : public OnOffApplication
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TimestampOnOffApplication")
      .SetParent<OnOffApplication> ()
      .AddConstructor<TimestampOnOffApplication> ();
    return tid;
  }

protected:
  virtual void SendPacket (void) override
  {
    NS_ASSERT (m_sendEvent.IsExpired ());
    Ptr<Packet> packet = Create<Packet> (m_pktSize);

    // Add timestamp tag
    PacketTagTime tag;
    tag.SetTime (Simulator::Now ());
    packet->AddPacketTag (tag);

    m_txTrace (packet);
    m_socket->Send (packet);
    if (!m_maxBytes || (m_totBytes < m_maxBytes))
      {
        ScheduleNextTx ();
      }
  }
};

int main (int argc, char *argv[])
{
  // Command-line argument defaults
  double distance = 50.0;
  std::string format = "omnet";
  uint32_t run = 1;
  uint32_t packetSize = 1024;
  std::string dataRate = "5Mbps";
  std::string phyMode = "HtMcs7";
  double simTime = 10.0;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("distance", "Distance (meters) between the two nodes", distance);
  cmd.AddValue ("format", "Output format: omnet or sqlite", format);
  cmd.AddValue ("run", "Run number for RNG seed", run);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("dataRate", "OnOffApplication data rate", dataRate);
  cmd.AddValue ("phyMode", "Physical mode (e.g., HtMcs7 for 802.11n)", phyMode);
  cmd.AddValue ("simTime", "Total simulation time (seconds)", simTime);
  cmd.Parse (argc, argv);

  RngSeedManager::SetSeed (12345);
  RngSeedManager::SetRun (run);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure Wi-Fi devices in Ad-hoc mode
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (phyMode),
                               "ControlMode", StringValue (phyMode));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Place the nodes at specified distance
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  Ptr<MobilityModel> mob0 = nodes.Get (0)->GetObject<MobilityModel> ();
  Ptr<MobilityModel> mob1 = nodes.Get (1)->GetObject<MobilityModel> ();
  mob0->SetPosition (Vector (0.0, 0.0, 0.0));
  mob1->SetPosition (Vector (distance, 0.0, 0.0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up a UDP server (packet sink) on node 1
  uint16_t port = 5000;
  ApplicationContainer serverApp;
  UdpServerHelper server (port);
  serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // Set up a UDP OnOff application with timestamp tagging on node 0
  Ptr<TimestampOnOffApplication> onoff = CreateObject<TimestampOnOffApplication> ();
  onoff->SetAttribute ("Protocol", StringValue ("ns3::UdpSocketFactory"));
  onoff->SetAttribute ("Remote", AddressValue (InetSocketAddress (interfaces.GetAddress (1), port)));
  onoff->SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff->SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  onoff->SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  nodes.Get (0)->AddApplication (onoff);
  onoff->SetStartTime (Seconds (1.0));
  onoff->SetStopTime (Seconds (simTime));

  // Connect callbacks for statistics collection
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/*/$TimestampOnOffApplication/Tx", MakeCallback (&TxCallback));

  Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (serverApp.Get (0));
  // Packet receive callback at UDP server
  // (We need to hook at MAC or UDP level for more details, but for statistics, we count Rx at server)
  // For per-packet delay (requires tag extraction):
  udpServer->TraceConnectWithoutContext ("RxWithAddresses",
    MakeCallback ([] (Ptr<const Packet> packet, const Address &from, const Address &to) {
      RxCallback (packet, from);
    }));

  // DataCollector configuration
  Ptr<DataCollector> dc = CreateObject<DataCollector> ();
  dc->SetExperimentDescription ("Wi-Fi Adhoc Distance Performance");
  dc->SetExperimentLabel ("wifi-distance");
  dc->SetInputConfiguration ("distance=" + std::to_string (distance));
  dc->SetRunLabel ("run" + std::to_string (run));
  dc->AddMetadata ("author", "ns-3 wifi experiment");

  // Collect results at the end of simulation
  Simulator::Schedule (Seconds (simTime + 0.001), [&] {
    dc->AddData ("distance", distance);
    dc->AddData ("run", run);
    dc->AddData ("format", format);
    dc->AddData ("TxPackets", g_txPackets);
    dc->AddData ("RxPackets", g_rxPackets);
    dc->AddData ("AverageDelay", g_rxPackets > 0 ? g_totalDelay / g_rxPackets : 0.0);
  });

  Simulator::Stop (Seconds (simTime + 0.01));
  Simulator::Run ();

  // Output
  if (format == "omnet")
    {
      OmnetDataOutput omnetOutput;
      omnetOutput.Serialize (*dc, "wifi-distance.omnet");
      std::cout << "Results saved in wifi-distance.omnet (OMNeT format)" << std::endl;
    }
  else if (format == "sqlite")
    {
      SqliteDataOutput sqliteOutput;
      sqliteOutput.Serialize (*dc, "wifi-distance.db");
      std::cout << "Results saved in wifi-distance.db (SQLite)" << std::endl;
    }
  else
    {
      std::cerr << "Unknown output format. Supported: omnet, sqlite" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}