#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/data-collection-module.h"
#include "ns3/omnet-data-output.h"
#include "ns3/sqlite-data-output.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDistancePerformanceExperiment");

struct PacketStats
{
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  uint64_t txBytes = 0;
  uint64_t rxBytes = 0;
  Time totalDelay = Seconds (0);
};

static PacketStats stats;

// Track each packet’s TX time to compute delay
static std::map<uint64_t, Time> packetTxTimes;
static uint64_t packetUid = 0;

void
TxCallback (Ptr<const Packet> packet)
{
  stats.txPackets++;
  stats.txBytes += packet->GetSize ();
  packetUid++;
  packetTxTimes[packetUid] = Simulator::Now ();
  // Store packet UID as tag
  struct UidTag : public Tag
  {
    uint64_t uid;
    void Serialize (TagBuffer i) const override { i.WriteU64 (uid); }
    void Deserialize (TagBuffer i) override { uid = i.ReadU64 (); }
    uint32_t GetSerializedSize () const override { return 8; }
    void Print (std::ostream &os) const override { os << "UID=" << uid; }
    static TypeId GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::UidTag").SetParent<Tag> ();
      return tid;
    }
    TypeId GetInstanceTypeId () const override { return GetTypeId (); }
  };
  UidTag tag;
  tag.uid = packetUid;
  Ptr<Packet> temp = packet->Copy ();
  temp->AddPacketTag (tag);
}

void
RxCallback (Ptr<const Packet> packet, const Address &addr)
{
  stats.rxPackets++;
  stats.rxBytes += packet->GetSize ();
  // Extract UID tag
  struct UidTag : public Tag
  {
    uint64_t uid;
    void Serialize (TagBuffer i) const override { i.WriteU64 (uid); }
    void Deserialize (TagBuffer i) override { uid = i.ReadU64 (); }
    uint32_t GetSerializedSize () const override { return 8; }
    void Print (std::ostream &os) const override { os << "UID=" << uid; }
    static TypeId GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::UidTag").SetParent<Tag> ();
      return tid;
    }
    TypeId GetInstanceTypeId () const override { return GetTypeId (); }
  };
  UidTag tag;
  Ptr<Packet> temp = packet->Copy ();
  if (temp->PeekPacketTag (tag))
    {
      auto it = packetTxTimes.find (tag.uid);
      if (it != packetTxTimes.end ())
        {
          stats.totalDelay += Simulator::Now () - it->second;
          packetTxTimes.erase (it);
        }
    }
}

int
main (int argc, char *argv[])
{
  double distance = 50.0;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  double interval = 0.01;
  std::string format = "omnet";
  uint32_t runId = 1;

  CommandLine cmd;
  cmd.AddValue ("distance", "Distance between nodes (meters)", distance);
  cmd.AddValue ("format", "Output format: omnet or sqlite", format);
  cmd.AddValue ("run", "Run identifier for reproducibility", runId);
  cmd.Parse (argc, argv);

  RngSeedManager::SetSeed (1);
  RngSeedManager::SetRun (runId);

  // Nodes
  NodeContainer nodes;
  nodes.Create (2);

  // WiFi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (distance, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet
  InternetStackHelper stack;
  stack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Applications
  uint16_t port = 50000;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  onoff.SetAttribute ("MaxBytes", UintegerValue (0));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("Interval", TimeValue (Seconds (interval)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // Connect callbacks for statistics
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::OnOffApplication/Tx", MakeCallback (&TxCallback));
  Config::ConnectWithoutContext ("/NodeList/1/ApplicationList/0/$ns3::PacketSink/RxWithAddresses", MakeCallback (&RxCallback));

  // Simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  // Data Collection
  DataCollector collector;
  collector.DescribeRun ("wifi-distance-exp", "WiFi Performance vs Distance", "Effect of distance on ad-hoc WiFi throughput and delay");
  collector.AddMetadata ("author", "AutoGenerated");
  collector.AddMetadata ("distance", std::to_string (distance));
  collector.AddMetadata ("run", std::to_string (runId));

  // Experiment metadata
  DataCollectionObjectFactory groupFactory;
  groupFactory.SetTypeId ("ns3::DataCollectionObject");
  Ptr<DataCollectionObject> group = groupFactory.Create ()->GetObject<DataCollectionObject> ();
  group->SetName ("Config");
  group->AddAttribute ("distance", DoubleValue (distance));
  group->AddAttribute ("run", UintegerValue (runId));
  collector.AddObject (group);

  // Statistic group
  DataCollectionObjectFactory statFactory;
  statFactory.SetTypeId ("ns3::DataCollectionObject");
  Ptr<DataCollectionObject> statGroup = statFactory.Create ()->GetObject<DataCollectionObject> ();
  statGroup->SetName ("Stats");
  statGroup->AddAttribute ("txPackets", UintegerValue (stats.txPackets));
  statGroup->AddAttribute ("rxPackets", UintegerValue (stats.rxPackets));
  statGroup->AddAttribute ("txBytes", UintegerValue (stats.txBytes));
  statGroup->AddAttribute ("rxBytes", UintegerValue (stats.rxBytes));
  statGroup->AddAttribute ("avgDelayMs", DoubleValue (
    stats.rxPackets == 0 ? 0.0 : stats.totalDelay.GetMilliSeconds () / stats.rxPackets));
  collector.AddObject (statGroup);

  // Output
  if (format == "omnet")
    {
      OmnetDataOutput omnetOut;
      omnetOut.Output (collector);
    }
  else if (format == "sqlite")
    {
      SqliteDataOutput sqliteOut;
      sqliteOut.Output (collector);
    }

  return 0;
}