#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExample");

class FixedRssHelper : public YansWifiPhyHelper
{
public:
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("ns3::FixedRssHelper")
                            .SetParent<YansWifiPhyHelper>()
                            .AddConstructor<FixedRssHelper>();
    return tid;
  }

  FixedRssHelper(double rss)
      : m_rss(rss)
  {
    SetErrorRateModel("ns3::NistErrorRateModel");
  }

  Ptr<WifiPhy> Create(Ptr<Node> node, Ptr<NetDevice> device) const override
  {
    Ptr<YansWifiPhy> phy = DynamicCast<YansWifiPhy>(YansWifiPhyHelper::Create(node, device));
    if (phy)
    {
      phy->SetRxGain(0); // Disable gain adjustments
      phy->SetTxPowerStart(0);
      phy->SetTxPowerEnd(0);
      phy->SetRxSensitivity(m_rss - 1); // Stop receiving when below RSS
    }
    return phy;
  }

private:
  double m_rss;
};

int main(int argc, char *argv[])
{
  double rss = -80;            // dBm
  uint32_t packetSize = 1000;  // bytes
  bool verbose = true;
  bool pcap = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
  cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue("verbose", "Enable verbose logging", verbose);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcap);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("AdhocWifiExample", LOG_LEVEL_INFO);
  }

  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes;
  nodes.Create(2);

  NS_LOG_INFO("Setting up mobility with constant position.");
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  Mac48Address bssid = Mac48Address::Allocate();

  Ssid ssid = Ssid("adhoc-network");

  mac.SetType("ns3::AdhocWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer devices;
  FixedRssHelper phyHelper(rss);
  devices = wifi.Install(phyHelper, mac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(2.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  if (pcap)
  {
    PointToPointHelper p2p;
    p2p.EnablePcapAll("adhoc-wifi-example");
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      phyHelper.EnablePcap("adhoc-wifi-example", nodes.Get(i)->GetId());
    }
  }

  Simulator::Stop(Seconds(5.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}