#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/packet-socket-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocFixedRssWifiExample");

// Fixed RSS propagation loss model, allows setting the received power explicitly
class FixedRssLossModel : public PropagationLossModel
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::FixedRssLossModel")
      .SetParent<PropagationLossModel> ()
      .SetGroupName ("Propagation")
      .AddConstructor<FixedRssLossModel> ()
      .AddAttribute ("Rss",
                     "The RSS (dBm) to be returned by this model",
                     DoubleValue (-80.0),
                     MakeDoubleAccessor (&FixedRssLossModel::m_rss),
                     MakeDoubleChecker<double> ());
    return tid;
  }

  FixedRssLossModel ()
    : m_rss (-80.0)
  { }
  
  void SetRss (double rss)
  {
    m_rss = rss;
  }
  double GetRss () const
  {
    return m_rss;
  }

private:
  // PropagationLossModel interface
  double DoCalcRxPower (double, Ptr<MobilityModel>, Ptr<MobilityModel>) const override
  {
    return m_rss;
  }
  int64_t DoAssignStreams (int64_t stream)
  {
    return 0;
  }

  double m_rss;
};

static void
RxCallback (std::string context, Ptr<const Packet> packet, const Address &)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s: Packet received of size " << packet->GetSize ());
}

int
main (int argc, char *argv[])
{
  double rss = -60.0; // dBm, default RSS
  double rxThreshold = -80.0; // dBm, default minimum Rx sensitivity
  uint32_t packetSize = 1000; // bytes
  bool verbose = false;
  bool enablePcap = false;
  std::string phyMode = "DsssRate11Mbps";
  uint32_t numPackets = 1;

  CommandLine cmd;
  cmd.AddValue ("rss", "Fixed RSS value in dBm (overrides node distance)", rss);
  cmd.AddValue ("rxThreshold", "Minimum Rx power (dBm) to successfully receive packets", rxThreshold);
  cmd.AddValue ("packetSize", "Size of each packet sent (bytes)", packetSize);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("phyMode", "Wifi Phy mode (e.g. DsssRate11Mbps, DsssRate1Mbps)", phyMode);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("AdhocFixedRssWifiExample", LOG_LEVEL_ALL);
      LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("WifiMac", LOG_LEVEL_INFO);
      LogComponentEnable ("WifiPhy", LOG_LEVEL_INFO);
      LogComponentEnable ("YansWifiPhy", LOG_LEVEL_INFO);
      LogComponentEnable ("YansWifiChannel", LOG_LEVEL_INFO);
      LogComponentEnable ("DcaTxop", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Wifi configuration
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  Ptr<FixedRssLossModel> fixedRss = CreateObject<FixedRssLossModel> ();
  fixedRss->SetRss (rss);

  PointerValue txGain;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FixedRssLossModel", "Rss", DoubleValue (rss));
  phy.SetChannel (channel.Create ());
  phy.Set ("RxGain", DoubleValue (0));
  phy.Set ("TxGain", DoubleValue (0));
  phy.Set ("CcaMode1Threshold", DoubleValue (-100.0));
  phy.Set ("RxSensitivity", DoubleValue (rxThreshold));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (phyMode),
                               "ControlMode", StringValue (phyMode));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  // Ad-hoc mode
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Application: send a packet from node 0 to node 1
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
                     InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));

  ApplicationContainer senderApp = onoff.Install (nodes.Get (0));

  // Sink on the receiving node
  PacketSinkHelper sink ("ns3::UdpSocketFactory", 
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (20.0));

  Config::Connect ("/NodeList/1/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&RxCallback));

  if (enablePcap)
    {
      phy.EnablePcap ("adhoc-fixed-rss-wifi", devices, true);
    }

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}