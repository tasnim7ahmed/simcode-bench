#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

struct WifiStandardBand
{
  WifiStandard standard;
  WifiPhyBand band;
};

WifiStandardBand ParseWifiVersion(const std::string& version)
{
  if (version == "80211a")
    return {WIFI_STANDARD_80211a, WIFI_PHY_BAND_5GHZ};
  if (version == "80211b")
    return {WIFI_STANDARD_80211b, WIFI_PHY_BAND_2_4GHZ};
  if (version == "80211g")
    return {WIFI_STANDARD_80211g, WIFI_PHY_BAND_2_4GHZ};
  if (version == "80211n-2.4")
    return {WIFI_STANDARD_80211n, WIFI_PHY_BAND_2_4GHZ};
  if (version == "80211n-5")
    return {WIFI_STANDARD_80211n, WIFI_PHY_BAND_5GHZ};
  if (version == "80211ac")
    return {WIFI_STANDARD_80211ac, WIFI_PHY_BAND_5GHZ};
  if (version == "80211ax-2.4")
    return {WIFI_STANDARD_80211ax, WIFI_PHY_BAND_2_4GHZ};
  if (version == "80211ax-5")
    return {WIFI_STANDARD_80211ax, WIFI_PHY_BAND_5GHZ};
  return {WIFI_STANDARD_80211g, WIFI_PHY_BAND_2_4GHZ};
}

void ThroughputMonitor(FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, double simulationTime)
{
  flowMon->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmhelper->GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMon->GetFlowStats();

  double rxBytes = 0.0;
  double timeSpan = simulationTime;
  for(auto iter = stats.begin(); iter != stats.end(); ++iter)
  {
    rxBytes += iter->second.rxBytes;
  }
  double throughput = (rxBytes * 8) / (timeSpan * 1e6); // Mbps
  std::cout << "Average throughput: " << throughput << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
  std::string apStandardStr = "80211ac";
  std::string staStandardStr = "80211g";
  std::string apRate = "Aarf";
  std::string staRate = "MinstrelHt";
  std::string trafficDirection = "up"; // "up", "down", "both"
  uint32_t packetSize = 1472;
  uint32_t numPackets = 5000;
  uint16_t port = 5000;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("apStandard", "WiFi standard for AP (e.g., 80211ac)", apStandardStr);
  cmd.AddValue("staStandard", "WiFi standard for STA (e.g., 80211g)", staStandardStr);
  cmd.AddValue("apRate", "Rate adaptation for AP", apRate);
  cmd.AddValue("staRate", "Rate adaptation for STA", staRate);
  cmd.AddValue("traffic", "Traffic direction: up/down/both", trafficDirection);
  cmd.AddValue("packetSize", "UDP packet size", packetSize);
  cmd.AddValue("numPackets", "Number of packets", numPackets);
  cmd.AddValue("simTime", "Simulation time (s)", simulationTime);
  cmd.Parse(argc, argv);

  WifiStandardBand apParams = ParseWifiVersion(apStandardStr);
  WifiStandardBand staParams = ParseWifiVersion(staStandardStr);

  NodeContainer wifiApNode, wifiStaNode;
  wifiApNode.Create(1);
  wifiStaNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

  // AP PHY/MAC setup
  YansWifiPhyHelper phyAp = YansWifiPhyHelper::Default();
  phyAp.SetChannel(channel.Create());
  WifiHelper wifiAp;
  wifiAp.SetStandard(apParams.standard);
  phyAp.Set("Frequency", UintegerValue(apParams.band == WIFI_PHY_BAND_2_4GHZ ? 2412 : 5180));
  if (apRate == "Aarf")
    wifiAp.SetRemoteStationManager("ns3::AarfWifiManager");
  else if (apRate == "MinstrelHt")
    wifiAp.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
  else
    wifiAp.SetRemoteStationManager("ns3::ConstantRateWifiManager");

  Ssid ssid = Ssid("compat-ssid");
  WifiMacHelper macAp;
  macAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifiAp.Install(phyAp, macAp, wifiApNode);

  // STA PHY/MAC setup
  YansWifiPhyHelper phySta = YansWifiPhyHelper::Default();
  phySta.SetChannel(channel.Create());
  WifiHelper wifiSta;
  wifiSta.SetStandard(staParams.standard);
  phySta.Set("Frequency", UintegerValue(staParams.band == WIFI_PHY_BAND_2_4GHZ ? 2412 : 5180));
  if (staRate == "Aarf")
    wifiSta.SetRemoteStationManager("ns3::AarfWifiManager");
  else if (staRate == "MinstrelHt")
    wifiSta.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
  else
    wifiSta.SetRemoteStationManager("ns3::ConstantRateWifiManager");

  WifiMacHelper macSta;
  macSta.SetType("ns3::StaWifiMac",
                 "Ssid", SsidValue(ssid),
                 "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifiSta.Install(phySta, macSta, wifiStaNode);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
  positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNode);

  // Internet
  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  // Global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  ApplicationContainer serverApps, clientApps;

  if (trafficDirection == "down" || trafficDirection == "both")
    {
      // UDP server on STA, client on AP
      UdpServerHelper udpServer(port);
      serverApps.Add(udpServer.Install(wifiStaNode.Get(0)));

      UdpClientHelper udpClient(staInterface.GetAddress(0), port);
      udpClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
      udpClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
      udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientApps.Add(udpClient.Install(wifiApNode.Get(0)));
    }

  if (trafficDirection == "up" || trafficDirection == "both")
    {
      // UDP server on AP, client on STA
      UdpServerHelper udpServer(port+1);
      serverApps.Add(udpServer.Install(wifiApNode.Get(0)));

      UdpClientHelper udpClient(apInterface.GetAddress(0), port+1);
      udpClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
      udpClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
      udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
      clientApps.Add(udpClient.Install(wifiStaNode.Get(0)));
    }

  serverApps.Start(Seconds(0.0));
  clientApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));
  clientApps.Stop(Seconds(simulationTime));

  // Flow monitor for throughput
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMon = flowHelper.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  ThroughputMonitor(&flowHelper, flowMon, simulationTime - 1.0); // 1s warmup

  Simulator::Destroy();
  return 0;
}