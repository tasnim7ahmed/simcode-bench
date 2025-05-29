#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <map>
#include <string>

using namespace ns3;

// Map SSID name for easy configuration
static const std::string kSsidString = "wifi-compat-ssid";

struct WifiStandardConfig
{
  WifiPhyStandard standard;
  WifiPhyBand band;
};

WifiStandardConfig
GetWifiStandardConfig(const std::string &version)
{
  static const std::map<std::string, std::pair<WifiPhyStandard, WifiPhyBand>> versionMap = {
      {"80211a", {WIFI_PHY_STANDARD_80211a, WIFI_PHY_BAND_5GHZ}},
      {"80211b", {WIFI_PHY_STANDARD_80211b, WIFI_PHY_BAND_2_4GHZ}},
      {"80211g", {WIFI_PHY_STANDARD_80211g, WIFI_PHY_BAND_2_4GHZ}},
      {"80211n-2.4GHz", {WIFI_PHY_STANDARD_80211n_2_4GHZ, WIFI_PHY_BAND_2_4GHZ}},
      {"80211n-5GHz", {WIFI_PHY_STANDARD_80211n_5GHZ, WIFI_PHY_BAND_5GHZ}},
      {"80211ac", {WIFI_PHY_STANDARD_80211ac, WIFI_PHY_BAND_5GHZ}},
      {"80211ax-2.4GHz", {WIFI_PHY_STANDARD_80211ax_2_4GHZ, WIFI_PHY_BAND_2_4GHZ}},
      {"80211ax-5GHz", {WIFI_PHY_STANDARD_80211ax_5GHZ, WIFI_PHY_BAND_5GHZ}}};

  auto search = versionMap.find(version);
  if (search != versionMap.end())
  {
    return {search->second.first, search->second.second};
  }
  else
  {
    // Default to 802.11g 2.4GHz
    return {WIFI_PHY_STANDARD_80211g, WIFI_PHY_BAND_2_4GHZ};
  }
}

void
ThroughputMonitor(Ptr<PacketSink> sink, double interval, double &lastTotalRx, Ptr<OutputStreamWrapper> stream)
{
  double totalRx = sink->GetTotalRx();
  double throughput = (totalRx - lastTotalRx) * 8.0 / 1e6 / interval; // Mbps
  *stream->GetStream() << Simulator::Now().GetSeconds() << "s\tThroughput: " << throughput << " Mbps" << std::endl;
  lastTotalRx = totalRx;
  Simulator::Schedule(Seconds(interval), &ThroughputMonitor, sink, interval, lastTotalRx, stream);
}

int main(int argc, char *argv[])
{
  // Configurable options
  std::string apVersion = "80211ac";
  std::string staVersion = "80211n-2.4GHz";
  std::string apRateAdapt = "ns3::MinstrelHtWifiManager";
  std::string staRateAdapt = "ns3::AarfWifiManager";
  bool trafficFromAp = false;
  bool trafficFromSta = true;
  double simulationTime = 10.0;
  uint32_t udpPacketSize = 1472;
  uint32_t udpNumPackets = 100000;
  double udpInterval = 0.001;

  CommandLine cmd;
  cmd.AddValue("apVersion", "Wi-Fi standard for AP", apVersion);
  cmd.AddValue("staVersion", "Wi-Fi standard for STA", staVersion);
  cmd.AddValue("apRateAdapt", "AP rate adaptation algorithm", apRateAdapt);
  cmd.AddValue("staRateAdapt", "STA rate adaptation algorithm", staRateAdapt);
  cmd.AddValue("trafficFromAp", "Generate traffic from AP", trafficFromAp);
  cmd.AddValue("trafficFromSta", "Generate traffic from STA", trafficFromSta);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  // Get Wi-Fi standards and bands
  WifiStandardConfig apConfig = GetWifiStandardConfig(apVersion);
  WifiStandardConfig staConfig = GetWifiStandardConfig(staVersion);

  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create(1);
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();

  // Allow different physical defaults on devices
  phy.SetChannel(channel.Create());

  // Configure AP PHY
  phy.Set("Frequency", UintegerValue(apConfig.band == WIFI_PHY_BAND_2_4GHZ ? 2412 : 5180));
  phy.Set("ChannelWidth", UintegerValue(apConfig.standard == WIFI_PHY_STANDARD_80211ac ? 80 : 20));

  WifiHelper wifiAp;
  wifiAp.SetStandard(apConfig.standard);
  wifiAp.SetRemoteStationManager(apRateAdapt);

  WifiMacHelper macAp;
  Ssid ssid = Ssid(kSsidString);
  macAp.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices = wifiAp.Install(phy, macAp, wifiApNode);

  // STA PHY - must reset Phy for different Wi-Fi versions/frequencies
  YansWifiPhyHelper staPhy = YansWifiPhyHelper::Default();
  staPhy.SetChannel(channel.Create());
  staPhy.Set("Frequency", UintegerValue(staConfig.band == WIFI_PHY_BAND_2_4GHZ ? 2412 : 5180));
  staPhy.Set("ChannelWidth", UintegerValue(staConfig.standard == WIFI_PHY_STANDARD_80211ac ? 80 : 20));

  WifiHelper wifiSta;
  wifiSta.SetStandard(staConfig.standard);
  wifiSta.SetRemoteStationManager(staRateAdapt);

  WifiMacHelper macSta;
  macSta.SetType("ns3::StaWifiMac",
                 "Ssid", SsidValue(ssid),
                 "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifiSta.Install(staPhy, macSta, wifiStaNode);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP position
  positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // STA position
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  // IP addressing
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  // UDP Server/Client setup
  uint16_t port = 4000;
  ApplicationContainer serverApp, clientApp;
  Ptr<PacketSink> sink = nullptr;

  if (trafficFromSta)
  {
    // AP is server, STA sends traffic
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), port));
    serverApp = packetSinkHelper.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1.0));
    sink = DynamicCast<PacketSink>(serverApp.Get(0));

    UdpClientHelper client(apInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(udpNumPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(udpInterval)));
    client.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));
  }

  if (trafficFromAp)
  {
    // STA is server, AP sends traffic
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
    ApplicationContainer staSinkApp = packetSinkHelper.Install(wifiStaNode.Get(0));
    staSinkApp.Start(Seconds(0.0));
    staSinkApp.Stop(Seconds(simulationTime + 1.0));
    if (!sink)
      sink = DynamicCast<PacketSink>(staSinkApp.Get(0));

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(udpNumPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(udpInterval)));
    client.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    ApplicationContainer apClientApp = client.Install(wifiApNode.Get(0));
    apClientApp.Start(Seconds(1.0));
    apClientApp.Stop(Seconds(simulationTime));
  }

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Throughput output
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("throughput.txt");
  *stream->GetStream() << "Time\tThroughput (Mbps)" << std::endl;

  double lastTotalRx = 0.0;
  if (sink)
  {
    Simulator::Schedule(Seconds(1.1), &ThroughputMonitor, sink, 1.0, lastTotalRx, stream);
  }

  Simulator::Stop(Seconds(simulationTime + 2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}