/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: ns-3 Wi-Fi 7 (802.11be) Throughput Simulation
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <vector>
#include <string>
#include <iomanip>
#include <map>

using namespace ns3;

// Input parameters (can also be passed as cmd args)
struct SimParams
{
  std::vector<uint8_t> mcsList = {0, 4, 11};     // Example MCSs
  std::vector<uint16_t> channelWidths = {20, 40, 80, 160}; // MHz
  std::vector<std::string> guardIntervals = {"he-0.8us", "he-1.6us", "he-3.2us"};
  std::vector<std::string> frequencies = {"2.4", "5", "6"}; // GHz bands
  bool uplinkOfdma = false;
  bool bspr = false;
  uint32_t nStations = 4;
  std::string trafficType = "udp";
  uint32_t payloadSize = 1472; // bytes for UDP
  uint32_t mpduBufferSize = 64; // for A-MPDU
  double simTime = 10.0; // seconds
  double maxThroughputMargin = 1.20; // 20% over theoretical max
  double minThroughputMargin = 0.50; // 50% of theoretical max
  // Additional PHY settings can go here
};

struct SimResult
{
  uint8_t mcs;
  uint16_t chWidth;
  std::string gi;
  std::string freq;
  double throughputMbps;
};

std::map<std::string, uint16_t> band2channel = {
    {"2.4", 1},
    {"5", 36},
    {"6", 5}
};

std::map<std::string, WifiPhyStandard> band2phy = {
    {"2.4", WIFI_PHY_STANDARD_80211be_EHT},
    {"5", WIFI_PHY_STANDARD_80211be_EHT},
    {"6", WIFI_PHY_STANDARD_80211be_EHT}
};

// Get GI duration in nanoseconds
uint16_t GetGuardIntervalNs(const std::string& gi)
{
  if (gi == "he-0.8us")
    return 800;
  if (gi == "he-1.6us")
    return 1600;
  if (gi == "he-3.2us")
    return 3200;
  NS_ABORT_MSG_UNLESS(false, "Unknown GI: " << gi);
  return 800;
}

// Simple theoretical PHY throughput estimation for 11be (approximate)
double
EstimateWiFi11bePhyThroughputMbps(uint8_t mcs, uint16_t chWidth, const std::string &gi, uint32_t nStreams = 1)
{
  // EHT MCS data rates for 160 MHz, GI 0.8us, 1 spatial stream
  // (Table: IEEE 802.11be D1.4)
  // For simplicity, only for SU EHT PHY, 1 SS, 0.8us GI, in Mbps
  static std::map<uint8_t, double> mcs2ratePer80MHz_08us = {
      {0,  86.7}, {1,   173.3}, {2,  259.6}, {3,  346.0}, {4,  519.9},
      {5,  693.6}, {6,  1040.8}, {7,  1219.2}, {8,  1397.6}, {9,  1561.6},
      {10, 1731.2}, {11, 1904.0}, {12, 2078.4} // EHT MCS 12, 1 SS
  };

  uint16_t n80s = chWidth / 80; // 1 for 80, 2 for 160, 0 for <80

  double baseRate = 0.0;

  if (chWidth < 80)
  {
    baseRate = mcs2ratePer80MHz_08us[mcs] * (chWidth / 80.0);
  }
  else
  {
    baseRate = mcs2ratePer80MHz_08us[mcs] * n80s;
  }

  if (gi == "he-1.6us")
  {
    baseRate *= 0.782; // ~0.8us to 1.6us
  }
  else if (gi == "he-3.2us")
  {
    baseRate *= 0.670; // ~0.8us to 3.2us
  }

  if (nStreams > 1)
  {
    baseRate *= nStreams;
  }

  return baseRate;
}

// Install traffic generators
void
InstallApplications(NodeContainer& apNodes, NodeContainer& staNodes,
                    const std::string& trafficType, uint32_t payloadSize, double simTime)
{
  // One OnOffApplication/UdpClient to each STA->AP for uplink test
  // or BulkSend for TCP
  uint16_t port = 9000;
  ApplicationContainer apps;

  // AP IPs setup
  Ipv4Address apAddress = Ipv4Address("10.1.1.1");
  uint32_t numStations = staNodes.GetN();

  for (uint32_t i = 0; i < numStations; ++i)
  {
    Ptr<Node> staNode = staNodes.Get(i);

    OnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(apAddress, port + i));
    onOff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onOff.SetAttribute("DataRate", StringValue("1Gbps"));
    onOff.SetAttribute("StartTime", TimeValue(Seconds(0.1)));
    onOff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(apAddress, port + i));
    bulk.SetAttribute("MaxBytes", UintegerValue(0));
    bulk.SetAttribute("SendSize", UintegerValue(payloadSize));

    if (trafficType == "udp")
    {
      apps.Add(onOff.Install(staNode));
    }
    else
    {
      apps.Add(bulk.Install(staNode));
    }

    // AP receiver
    PacketSinkHelper sink(trafficType == "udp" ? "ns3::UdpSocketFactory" : "ns3::TcpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port + i));
    apps.Add(sink.Install(apNodes.Get(0)));
  }
}

// Simulation run for a given parameter set
SimResult
RunSingleSim(uint8_t mcs, uint16_t chWidth, const std::string& gi, const std::string& freq,
             const SimParams& user, uint32_t nStreams = 1)
{
  // Wi-Fi channel/freq setup
  WifiHelper wifi;
  WifiMacHelper wifiMac;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();

  phy.SetChannel(channel.Create());
  phy.Set("ChannelWidth", UintegerValue(chWidth));
  phy.Set("Frequency", UintegerValue(freq == "2.4" ? 2412 : (freq == "5" ? 5180 : 5955)));
  phy.Set("GuardInterval", TimeValue(NanoSeconds(GetGuardIntervalNs(gi)))); // For 11be

  // Wi-Fi 7 PHY/MAC
  wifi.SetStandard(WIFI_PHY_STANDARD_80211be_EHT);

  WifiEhtMacHelper ehtMac;
  ehtMac.SetType("ns3::StaWifiMac",
                 "Ssid", SsidValue(Ssid("ns3-eht")),
                 "QosSupported", BooleanValue(true));
  ehtMac.SetType("ns3::ApWifiMac",
                 "Ssid", SsidValue(Ssid("ns3-eht")),
                 "QosSupported", BooleanValue(true));

  // Set EHT MCS
  HtWifiMacHelper::SetHtSupported(wifiMac, true); // Enable MAC
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("HeMcs" + std::to_string(mcs)),
                              "ControlMode", StringValue("HeMcs" + std::to_string(mcs)));

  // Network topology
  NodeContainer staNodes, apNode;
  staNodes.Create(user.nStations);
  apNode.Create(1);

  // PHY->MAC
  NetDeviceContainer staDevs = wifi.Install(phy, ehtMac, staNodes);
  NetDeviceContainer apDevs = wifi.Install(phy, ehtMac, apNode);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);
  mobility.Install(apNode);

  // Internet
  InternetStackHelper stack;
  stack.Install(staNodes);
  stack.Install(apNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevs);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevs);

  // App setup
  InstallApplications(apNode, staNodes, user.trafficType, user.payloadSize, user.simTime);

  // Flow monitor for throughput stats
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(user.simTime + 0.5));
  Simulator::Run();

  // Collect RX stats
  double totalRxBytes = 0.0;
  double throughputMbps = 0.0;

  monitor->CheckForLostPackets();
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (auto const& i : stats)
  {
    if (i.second.rxPackets > 0)
    {
      double rx = i.second.rxBytes * 8.0 / (user.simTime * 1e6);
      throughputMbps += rx;
    }
  }
  // Normalize to per-station Mbps (uplink)
  throughputMbps = throughputMbps / user.nStations;

  Simulator::Destroy();

  // Theoretical maximum (1 SS for now, can be extended to multi-SS by user param)
  double expectedMax = EstimateWiFi11bePhyThroughputMbps(mcs, chWidth, gi, nStreams);
  double minThreshold = expectedMax * user.minThroughputMargin;
  double maxThreshold = expectedMax * user.maxThroughputMargin;

  if (throughputMbps < minThreshold)
  {
    NS_LOG_ERROR("Throughput too low: " << throughputMbps << " Mbps (min " << minThreshold << "), terminating.");
    exit(1);
  }
  else if (throughputMbps > maxThreshold)
  {
    NS_LOG_ERROR("Throughput too high: " << throughputMbps << " Mbps (max " << maxThreshold << "), terminating.");
    exit(1);
  }

  SimResult result = {mcs, chWidth, gi, freq, throughputMbps};
  return result;
}


int
main(int argc, char *argv[])
{
  SimParams user;

  // Allow command line override
  CommandLine cmd;
  cmd.AddValue("nStations", "Number of STA nodes", user.nStations);
  cmd.AddValue("payloadSize", "App payload size (bytes)", user.payloadSize);
  cmd.AddValue("trafficType", "Type of traffic to use (tcp|udp)", user.trafficType);
  cmd.AddValue("mpduBufferSize", "MPDU buffer size for aggregation", user.mpduBufferSize);
  cmd.AddValue("uplinkOfdma", "Enable 11be uplink OFDMA", user.uplinkOfdma);
  cmd.AddValue("bspr", "Enable 11be BSRP", user.bspr);
  cmd.AddValue("simTime", "Simulation time (seconds)", user.simTime);
  cmd.Parse(argc, argv);

  // Print header
  std::cout << std::setw(5) << "MCS"
            << std::setw(10) << "Width"
            << std::setw(10) << "GI"
            << std::setw(6)  << "Freq"
            << std::setw(15) << "Throughput(Mbps)"
            << std::endl;

  for (const auto& freq : user.frequencies)
  {
    for (const auto& mcs : user.mcsList)
    {
      for (const auto& chWidth : user.channelWidths)
      {
        // Skip 160 MHz if freq is 2.4
        if (freq == "2.4" && chWidth > 40)
          continue;
        for (const auto& gi : user.guardIntervals)
        {
          SimResult result = RunSingleSim(mcs, chWidth, gi, freq, user);

          std::cout << std::setw(5) << static_cast<int>(result.mcs)
                    << std::setw(10) << result.chWidth
                    << std::setw(10) << result.gi
                    << std::setw(6)  << result.freq
                    << std::setw(15) << std::fixed << std::setprecision(2) << result.throughputMbps
                    << std::endl;
        }
      }
    }
  }

  return 0;
}