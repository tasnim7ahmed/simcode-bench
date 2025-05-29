#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-helper.h"
#include <unordered_map>
#include <string>
#include <tuple>

using namespace ns3;

struct WifiConfig
{
  WifiStandard standard;
  WifiPhyBand band;
};

WifiConfig
GetWifiStandardAndBand(const std::string &standardStr)
{
  static const std::unordered_map<std::string, std::tuple<WifiStandard, WifiPhyBand>> mapping = {
      {"80211a", {WIFI_STANDARD_80211a, WIFI_PHY_BAND_5GHZ}},
      {"80211b", {WIFI_STANDARD_80211b, WIFI_PHY_BAND_2_4GHZ}},
      {"80211g", {WIFI_STANDARD_80211g, WIFI_PHY_BAND_2_4GHZ}},
      {"80211n_2_4", {WIFI_STANDARD_80211n, WIFI_PHY_BAND_2_4GHZ}},
      {"80211n_5", {WIFI_STANDARD_80211n, WIFI_PHY_BAND_5GHZ}},
      {"80211ac", {WIFI_STANDARD_80211ac, WIFI_PHY_BAND_5GHZ}},
      {"80211ax_2_4", {WIFI_STANDARD_80211ax, WIFI_PHY_BAND_2_4GHZ}},
      {"80211ax_5", {WIFI_STANDARD_80211ax, WIFI_PHY_BAND_5GHZ}}};
  auto it = mapping.find(standardStr);
  if (it != mapping.end())
    {
      auto [std, band] = it->second;
      return {std, band};
    }
  else
    {
      NS_ABORT_MSG("Unknown Wi-Fi version: " << standardStr);
    }
}

void
PrintThroughput(Ptr<FlowMonitor> flowMon, FlowMonitorHelper* flowHelper)
{
  double totalThroughput = 0.0;
  auto stats = flowMon->GetFlowStats();
  for (const auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = flowHelper->GetClassifier()->FindFlow(flow.first);
      double throughput = flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6;
      std::cout << "Flow " << flow.first
                << " (" << t.sourceAddress << "->" << t.destinationAddress << ")"
                << " Throughput: " << throughput << " Mbps" << std::endl;
      totalThroughput += throughput;
    }
  std::cout << "Aggregate Throughput: " << totalThroughput << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
  std::string apStandardStr = "80211ac";
  std::string staStandardStr = "80211g";
  std::string apRate = "AarfWifiManager";
  std::string staRate = "MinstrelWifiManager";
  std::string traffic = "ap"; // "ap", "sta", or "both"
  double simulationTime = 10.0; // seconds

  CommandLine cmd;
  cmd.AddValue("apStandard", "WiFi standard string for AP", apStandardStr);
  cmd.AddValue("staStandard", "WiFi standard string for STA", staStandardStr);
  cmd.AddValue("apRate", "Rate manager for AP", apRate);
  cmd.AddValue("staRate", "Rate manager for STA", staRate);
  cmd.AddValue("traffic", "Traffic direction: ap, sta, or both", traffic);
  cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);
  cmd.Parse(argc, argv);

  WifiConfig apCfg = GetWifiStandardAndBand(apStandardStr);
  WifiConfig staCfg = GetWifiStandardAndBand(staStandardStr);

  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create(1);
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager(apRate);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi-bw");

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  wifi.SetStandard(apCfg.standard);
  phy.Set("Frequency", UintegerValue(apCfg.band == WIFI_PHY_BAND_5GHZ ? 5180 : 2412));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Separate STA configuration for backward compatibility
  YansWifiPhyHelper staPhy = phy;
  WifiHelper staWifi = wifi;
  staWifi.SetRemoteStationManager(staRate);
  staWifi.SetStandard(staCfg.standard);
  staPhy.Set("Frequency", UintegerValue(staCfg.band == WIFI_PHY_BAND_5GHZ ? 5180 : 2412));
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = staWifi.Install(staPhy, mac, wifiStaNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(5.0),
                               "GridWidth", UintegerValue(1),
                               "LayoutType", StringValue("RowFirst"));
  mobility.Install(wifiApNode);
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(20.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(5.0),
                               "GridWidth", UintegerValue(1),
                               "LayoutType", StringValue("RowFirst"));
  mobility.Install(wifiStaNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);

  uint16_t port = 9;
  ApplicationContainer serverApps, clientApps;

  if (traffic == "ap" || traffic == "both")
    {
      UdpServerHelper server(port);
      serverApps.Add(server.Install(wifiStaNode.Get(0)));
      serverApps.Start(Seconds(1.0));
      serverApps.Stop(Seconds(simulationTime));

      UdpClientHelper client(staInterfaces.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(10000000));
      client.SetAttribute("Interval", TimeValue(MilliSeconds(0.1)));
      client.SetAttribute("PacketSize", UintegerValue(1472));
      clientApps.Add(client.Install(wifiApNode.Get(0)));
      clientApps.Start(Seconds(2.0));
      clientApps.Stop(Seconds(simulationTime));
    }

  if (traffic == "sta" || traffic == "both")
    {
      UdpServerHelper server(port+1);
      serverApps.Add(server.Install(wifiApNode.Get(0)));
      serverApps.Start(Seconds(1.0));
      serverApps.Stop(Seconds(simulationTime));

      UdpClientHelper client(apInterfaces.GetAddress(0), port+1);
      client.SetAttribute("MaxPackets", UintegerValue(10000000));
      client.SetAttribute("Interval", TimeValue(MilliSeconds(0.1)));
      client.SetAttribute("PacketSize", UintegerValue(1472));
      clientApps.Add(client.Install(wifiStaNode.Get(0)));
      clientApps.Start(Seconds(2.0));
      clientApps.Stop(Seconds(simulationTime));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMon = flowHelper.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  flowMon->CheckForLostPackets();
  PrintThroughput(flowMon, &flowHelper);

  Simulator::Destroy();
  return 0;
}