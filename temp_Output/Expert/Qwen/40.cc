#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiFixedRss");

class RssThresholdCallback : public Object {
public:
  static TypeId GetTypeId(void) {
    static TypeId tid = TypeId("RssThresholdCallback")
      .SetParent<Object>()
      .AddConstructor<RssThresholdCallback>();
    return tid;
  }

  bool ReceiveOk(Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t channelFreqMhz,
                 WifiTxVector txVector, MpduInfo aMpduInfo, SignalNoiseDbm signalNoise,
                 uint16_t staId) {
    double rssi = signalNoise.signal;
    if (rssi < m_minRss) {
      NS_LOG_VERBOSE("Dropping packet due to RSS below threshold (" << rssi << " < " << m_minRss << ")");
      return false;
    }
    NS_LOG_VERBOSE("Receiving packet with RSS: " << rssi);
    return true;
  }

  void SetMinRss(double r) { m_minRss = r; }

private:
  double m_minRss{-90.0};
};

int main(int argc, char *argv[]) {
  double rss{ -50.0 };
  double minRss{ -90.0 };
  uint32_t packetSize{ 1000 };
  bool verbose{ false };
  bool pcapTracing{ false };

  CommandLine cmd(__FILE__);
  cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
  cmd.AddValue("minRss", "Minimum RSS for reception (dBm)", minRss);
  cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue("verbose", "Enable verbose output", verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("AdhocWifiFixedRss", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State", StringValue("ns3::WifiPhyState::RX"));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(2.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 9)));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));

  RssThresholdCallback callback;
  callback.SetMinRss(minRss);
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx",
                  MakeCallback(&RssThresholdCallback::ReceiveOk, &callback));

  phy.EnablePcapAll("adhoc-wifi-fixed-rss", pcapTracing);

  Simulator::Stop(Seconds(3.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}