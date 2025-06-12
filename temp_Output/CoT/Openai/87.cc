#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/data-collection-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDistancePerformance");

uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;
double   g_totalDelay = 0.0;

void
TxCallback (Ptr<const Packet> p)
{
  ++g_txPackets;
}

void
RxCallback (Ptr<const Packet> packet, const Address &address, const Address &from)
{
  ++g_rxPackets;
}

void
RxTraceWithDelay (Ptr<const Packet> packet, const Address &address, const Address &from)
{
  static std::map<uint64_t, Time> sendTimes;
  uint64_t uid = packet->GetUid();
  if (sendTimes.find(uid) != sendTimes.end())
    {
      Time delay = Simulator::Now() - sendTimes[uid];
      g_totalDelay += delay.GetSeconds();
      sendTimes.erase(uid);
    }
}

void
AppTxTrace (Ptr<const Packet> packet)
{
  static std::map<uint64_t, Time> sendTimes;
  sendTimes[packet->GetUid()] = Simulator::Now();
}

// Helper function for DataCollector
void
ConfigureDataCollector (Ptr<DataCollector> dc, uint32_t run, double distance, std::string format)
{
  dc->SetExperimentId ("WiFiDistance");
  dc->SetRunId (std::to_string(run));
  dc->SetCollectionId ("distance=" + std::to_string(distance));
  dc->SetMetadata ("experiment", "WifiDistancePerformance");
  dc->SetMetadata ("distance", std::to_string(distance));
  if (format == "sqlite")
    {
      dc->ConfigureOutput (DataOutput::SQLITE, "wifi-distance.db", DataOutput::OMNET_FORMAT);
    }
  else
    {
      dc->ConfigureOutput (DataOutput::OMNET, "wifi-distance.omnet", DataOutput::OMNET_FORMAT);
    }
}

int
main (int argc, char *argv[])
{
  double distance = 10.0;
  uint32_t run = 1;
  std::string format = "sqlite";
  double simTime = 10.0; // seconds
  std::string phyMode = "DsssRate11Mbps";
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("distance", "Separation between nodes (meters)", distance);
  cmd.AddValue ("run", "Run identifier", run);
  cmd.AddValue ("format", "Output format: 'sqlite' or 'omnet'", format);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiDistancePerformance", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (phyMode), "ControlMode", StringValue (phyMode));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0.0, 0.0, 0.0));
  posAlloc->Add (Vector (distance, 0.0, 0.0));
  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP Server (node 1)
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // UDP Client (node 0)
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simTime));

  // Tracing & statistics
  Ptr<DataCollector> dataCollector = CreateObject<DataCollector> ();
  ConfigureDataCollector (dataCollector, run, distance, format);

  // Packet traces
  // Transmission -- connect to UDP Client Tx trace
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback (&TxCallback));
  Config::ConnectWithoutContext ("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback (&AppTxTrace));

  // Reception -- connect to UDP Server Rx trace
  Config::ConnectWithoutContext ("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback (&RxCallback));

  // Statistics variables will be saved by DataCollector at end of run
  // Custom Statistic
  struct TxRxDelayStat : public DataCalculator
  {
    virtual void DoDispose (void) {}
    virtual void Update () {}
    virtual void Output (Ptr<DataOutput> output)
    {
      // Output statistics
      std::map<std::string, DataCollectionObject> m;
      m["txPackets"] = DataCollectionObject ("txPackets", "uint32_t", g_txPackets);
      m["rxPackets"] = DataCollectionObject ("rxPackets", "uint32_t", g_rxPackets);
      double meanDelay = 0;
      if (g_rxPackets > 0)
        meanDelay = g_totalDelay / g_rxPackets;
      m["meanDelayS"] = DataCollectionObject ("meanDelayS", "double", meanDelay);
      output->Output (GetStreamId (), m);
    }
    virtual std::string GetStreamId () const { return "WifiExperimentStats"; }
    virtual void Reset () {}
  };

  Ptr<TxRxDelayStat> stat = CreateObject<TxRxDelayStat> ();
  dataCollector->AddDataCalculator (stat);

  Simulator::Stop (Seconds (simTime + 0.1));
  Simulator::Run ();

  dataCollector->SerializeToOutput ();

  Simulator::Destroy ();
  return 0;
}