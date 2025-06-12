#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocFixedRssWifiExample");

class FixedRssPropagationLossModel : public PropagationLossModel
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::FixedRssPropagationLossModel")
      .SetParent<PropagationLossModel> ()
      .SetGroupName("Propagation")
      .AddConstructor<FixedRssPropagationLossModel> ()
      .AddAttribute ("Rss", "Received signal strength in dBm.",
                     DoubleValue (-80.0),
                     MakeDoubleAccessor (&FixedRssPropagationLossModel::m_rss),
                     MakeDoubleChecker<double> ());
    return tid;
  }

  FixedRssPropagationLossModel ()
    : PropagationLossModel (),
      m_rss (-80.0)
  {}

  void SetRss (double rss)
  {
    m_rss = rss;
  }

protected:
  virtual double DoCalcRxPower (double txPowerDbm, Ptr<MobilityModel> a, Ptr<MobilityModel> b) const
  {
    (void) txPowerDbm; (void)a; (void)b;
    return m_rss;
  }
  virtual int64_t DoAssignStreams (int64_t stream)
  {
    return 0;
  }
private:
  double m_rss;
};

NS_OBJECT_ENSURE_REGISTERED (FixedRssPropagationLossModel);

int main (int argc, char *argv[])
{
  double rss = -80.0;
  double rxThreshold = -91.0;
  uint32_t packetSize = 1000;
  bool verbose = false;
  std::string pcapFile = "adhoc-fixed-rss";
  uint32_t numPackets = 1;
  double interPacketInterval = 1.0; // In seconds

  CommandLine cmd;
  cmd.AddValue ("rss", "Fixed RSS value in dBm", rss);
  cmd.AddValue ("rxThreshold", "Phy Rx sensitivity in dBm", rxThreshold);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("verbose", "Set verbose logging", verbose);
  cmd.AddValue ("pcapFile", "Base name for pcap output", pcapFile);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interPacketInterval", "Inter-packet interval (s)", interPacketInterval);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("AdhocFixedRssWifiExample", LOG_LEVEL_INFO);
      LogComponentEnable ("YansWifiPhy", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("DsssRate11Mbps"),
                               "ControlMode", StringValue ("DsssRate11Mbps"));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxSensitivity", DoubleValue (rxThreshold));

  Ptr<FixedRssPropagationLossModel> fixedRss = CreateObject<FixedRssPropagationLossModel> ();
  fixedRss->SetRss (rss);
  Ptr<YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel> ();
  wifiChannel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());
  wifiChannel->SetPropagationLossModel (fixedRss);

  wifiPhy.SetChannel (wifiChannel);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifs = ipv4.Assign (devices);

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (100.0));

  UdpClientHelper client (ifs.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (100.0));

  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcap (pcapFile, devices);

  Simulator::Stop (Seconds (102.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}