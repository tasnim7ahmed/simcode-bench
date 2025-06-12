#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAdhocFixedRssExample");

class FixedRssWifiPhy : public YansWifiPhy
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("FixedRssWifiPhy")
      .SetParent<YansWifiPhy> ()
      .SetGroupName ("Wifi")
      .AddConstructor<FixedRssWifiPhy> ()
      .AddAttribute ("FixedRss",
                     "Received Signal Strength (dBm) to be used for all packets",
                     DoubleValue (-60.0),
                     MakeDoubleAccessor (&FixedRssWifiPhy::m_fixedRss),
                     MakeDoubleChecker<double> ())
      ;
    return tid;
  }

  virtual double CalculateSnr (Ptr<const Packet> packet,
                               double txPowerDbm,
                               WifiTxVector txVector,
                               uint8_t channelWidth,
                               uint8_t nss,
                               bool isShortGuardInterval)
  {
    // Use fixed RSS, and propagate through to SNR calculation
    Ptr<MobilityModel> receiver = this->GetMobility ();
    double rxPowerW = DbmToW (m_fixedRss);
    double noiseW = this->GetNoiseDbm (this->m_channelNumber, channelWidth, nss);
    double snr = rxPowerW / noiseW;
    return snr;
  }

  double DbmToW (double dbm) const
  {
    return std::pow (10.0, dbm / 10.0) / 1000.0;
  }

  // Override to drop packet if RSS < threshold
  virtual bool IsReceptionPossible (Ptr<const Packet> packet,
                                    double rxPowerDbm,
                                    WifiTxVector txVector,
                                    uint16_t channelWidth,
                                    uint16_t nss)
  {
    if (m_fixedRss < GetRxSensitivity ())
      {
        return false;
      }
    return YansWifiPhy::IsReceptionPossible (packet, m_fixedRss, txVector, channelWidth, nss);
  }

  void SetFixedRss (double dbm)
  {
    m_fixedRss = dbm;
  }
private:
  double m_fixedRss;
};

int main (int argc, char *argv[])
{
  uint32_t packetSize = 1000;
  double rss = -60.0;
  bool verbose = false;
  std::string phyMode ("DsssRate11Mbps");
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of application packet sent (bytes)", packetSize);
  cmd.AddValue ("rss", "Fixed RSS value in dBm", rss);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.AddValue ("phyMode", "802.11b mode (e.g. DsssRate11Mbps)", phyMode);
  cmd.AddValue ("pcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiAdhocFixedRssExample", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  // We will create and use our customized physical layer:
  Ptr<YansWifiChannel> phyChannel = channel.Create ();

  Ptr<FixedRssWifiPhy> phy0 = CreateObject<FixedRssWifiPhy> ();
  phy0->SetChannel (phyChannel);
  phy0->Set ("FixedRss", DoubleValue (rss));
  phy0->Set ("RxSensitivity", DoubleValue (-85.0)); // 802.11b min sens

  Ptr<FixedRssWifiPhy> phy1 = CreateObject<FixedRssWifiPhy> ();
  phy1->SetChannel (phyChannel);
  phy1->Set ("FixedRss", DoubleValue (rss));
  phy1->Set ("RxSensitivity", DoubleValue (-85.0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices;

  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> n1 = nodes.Get (1);

  Ptr<NetDevice> dev0 = wifi.Install (phy0, mac, n0);
  Ptr<NetDevice> dev1 = wifi.Install (phy1, mac, n1);

  devices.Add (dev0);
  devices.Add (dev1);

  // Mobility: nodes are static, positions arbitrary
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);

  uint16_t port = 9;
  UdpEchoServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper client (ifaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (enablePcap)
    {
      phy0->EnablePcap ("adhoc-node0", dev0);
      phy1->EnablePcap ("adhoc-node1", dev1);
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}