#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationDemo");

struct NetworkConfig
{
  std::string ssid;
  uint16_t channelNumber;
  std::string apMac;
  std::string staMac;
  uint32_t ampduSize;
  bool ampduEnable;
  uint32_t amsduSize;
  bool amsduEnable;
  bool aggregationEnable;
};

struct NetStats
{
  double throughput;
  Time maxTxop;
  NetStats() : throughput(0.0), maxTxop(Seconds(0)) {}
};

void
TxopTrace(Time start, Time end, uint32_t netId, NetStats* stats)
{
  Time duration = end - start;
  if (duration > stats->maxTxop)
    stats->maxTxop = duration;
}

static void
CalculateThroughput(Ptr<PacketSink> sink, NetStats* stats, double simulationTime)
{
  uint64_t totalBytes = sink->GetTotalRx();
  stats->throughput = (totalBytes * 8.0) / (simulationTime * 1000000.0); // Mbps
}

int main(int argc, char* argv[])
{
  double distance = 10.0;
  bool enableRtsCts = false;
  double txopLimitMicroseconds = 0.0;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue("distance", "Distance between AP and STA", distance);
  cmd.AddValue("enableRtsCts", "Enable/disable RTS/CTS", enableRtsCts);
  cmd.AddValue("txop", "TXOP limit in microseconds (0 = default per AC)", txopLimitMicroseconds);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  std::vector<NetworkConfig> configs = {
      {"network-A", 36, "00:00:00:00:01:01", "00:00:00:00:01:11", 65535, true, 0, false, true},
      {"network-B", 40, "00:00:00:00:02:01", "00:00:00:00:02:11", 0, false, 0, false, false},
      {"network-C", 44, "00:00:00:00:03:01", "00:00:00:00:03:11", 0, false, 8192, true, true},
      {"network-D", 48, "00:00:00:00:04:01", "00:00:00:00:04:11", 32768, true, 4096, true, true},
  };
  const uint32_t numNets = configs.size();

  std::vector<NodeContainer> aps(numNets), stas(numNets);
  std::vector<Ptr<YansWifiChannel>> channels(numNets);
  std::vector<NetDeviceContainer> apDevices(numNets), staDevices(numNets);
  std::vector<Ssid> ssids(numNets);
  std::vector<ApplicationContainer> appContainers(numNets);
  std::vector<Ptr<PacketSink>> sinks(numNets);
  std::vector<NetStats> netStats(numNets);

  NodeContainer allNodes;

  Ptr<UniformRandomVariable> rv = CreateObject<UniformRandomVariable>();

  // Install Wi-Fi networks
  for (uint32_t i = 0; i < numNets; ++i)
  {
    aps[i].Create(1);
    stas[i].Create(1);
    allNodes.Add(aps[i]);
    allNodes.Add(stas[i]);

    // PHY/Channel per network
    channels[i] = CreateObject<YansWifiChannel>();
    Ptr<YansWifiPhy> apPhy = CreateObject<YansWifiPhy>();
    Ptr<YansWifiPhy> staPhy = CreateObject<YansWifiPhy>();
    apPhy->SetChannel(channels[i]);
    staPhy->SetChannel(channels[i]);

    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channels[i]);
    phyHelper.Set("ShortGuardEnabled", BooleanValue(true));
    phyHelper.Set("ChannelNumber", UintegerValue(configs[i].channelNumber));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    ssids[i] = Ssid(configs[i].ssid);

    // Setup AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssids[i]),
                "QosSupported", BooleanValue(true));

    std::ostringstream apMacStr;
    apMacStr << configs[i].apMac;

    phyHelper.Set("ChannelNumber", UintegerValue(configs[i].channelNumber));
    apDevices[i] = wifi.Install(phyHelper, mac, aps[i]);

    // Setup STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssids[i]),
                "QosSupported", BooleanValue(true));
    staDevices[i] = wifi.Install(phyHelper, mac, stas[i]);

    // Set custom MAC addresses
    apDevices[i].Get(0)->SetAddress(Mac48Address::Allocate());
    staDevices[i].Get(0)->SetAddress(Mac48Address::Allocate());

    // Aggregation/11n setup on STA side
    Config::Set("/NodeList/" + std::to_string(stas[i].Get(0)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN", StringValue("ns3::EdcaTxopN"));
    std::ostringstream ampduPath, amsduPath, aggrPath;
    uint32_t staId = stas[i].Get(0)->GetId();

    ampduPath << "/NodeList/" << staId << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/Ampdu";
    amsduPath << "/NodeList/" << staId << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/Amsdu";
    aggrPath << "/NodeList/" << staId << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN";

    Config::Set(aggrPath.str() + "/EnableAggregation", BooleanValue(configs[i].aggregationEnable));
    Config::Set(ampduPath.str() + "/Enable", BooleanValue(configs[i].ampduEnable));
    if (configs[i].ampduEnable && configs[i].ampduSize > 0)
      Config::Set(ampduPath.str() + "/MaxAmpduSize", UintegerValue(configs[i].ampduSize));
    Config::Set(amsduPath.str() + "/Enable", BooleanValue(configs[i].amsduEnable));
    if (configs[i].amsduEnable && configs[i].amsduSize > 0)
      Config::Set(amsduPath.str() + "/MaxAmsduSize", UintegerValue(configs[i].amsduSize));
  }

  // Mobility - 1 AP and 1 STA, separate for each network
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
  double xGap = 40;

  for (uint32_t i = 0; i < numNets; ++i)
  {
    posAlloc->Add(Vector((i+1) * xGap, 0, 0));                   // AP
    posAlloc->Add(Vector((i+1) * xGap, distance, 0));            // STA
  }

  mobility.SetPositionAllocator(posAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  InternetStackHelper internet;
  internet.Install(allNodes);

  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> interfaces(numNets);

  for (uint32_t i = 0; i < numNets; ++i)
  {
    std::ostringstream subnet;
    subnet << "10." << (i+1) << ".0.0";
    ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
    NodeContainer pair;
    pair.Add(aps[i]);
    pair.Add(stas[i]);
    NetDeviceContainer devices;
    devices.Add(apDevices[i].Get(0));
    devices.Add(staDevices[i].Get(0));
    interfaces[i] = ipv4.Assign(devices);
    ipv4.NewNetwork();
  }

  // Disable/enable RTS/CTS
  WifiMacHelper::Default();
  if (enableRtsCts)
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
  else
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));

  // TXOP settings (on both AP and STA)
  if (txopLimitMicroseconds > 0.0)
  {
    for (uint32_t i = 0; i < numNets; ++i)
    {
      std::ostringstream apTxopPath, staTxopPath;
      apTxopPath << "/NodeList/" << aps[i].Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/BE_EdcaTxopN/TxopLimit";
      staTxopPath << "/NodeList/" << stas[i].Get(0)->GetId() << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/TxopLimit";
      Config::Set(apTxopPath.str(), TimeValue(MicroSeconds(txopLimitMicroseconds)));
      Config::Set(staTxopPath.str(), TimeValue(MicroSeconds(txopLimitMicroseconds)));
    }
  }

  // Applications: UDP traffic uplink from STA to AP
  uint16_t port = 5000;
  for (uint32_t i = 0; i < numNets; ++i)
  {
    UdpServerHelper server(port);
    appContainers[i].Add(server.Install(aps[i].Get(0)));
    UdpClientHelper client(interfaces[i].GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100))); // High load
    client.SetAttribute("PacketSize", UintegerValue(1472)); // Typical max UDP size for AMSDU/AMPDU demo
    appContainers[i].Add(client.Install(stas[i].Get(0)));
    sinks[i] = DynamicCast<PacketSink>(appContainers[i].Get(0).Get(0));
    appContainers[i].Start(Seconds(0.1));
    appContainers[i].Stop(Seconds(simulationTime));
  }

  // Trace TXOP durations for AP MACs
  std::vector<Ptr<EventId>> txopEvents(numNets);
  for (uint32_t i = 0; i < numNets; ++i)
  {
    std::ostringstream txStart, txEnd;
    auto& stats = netStats[i];
    Ptr<NetDevice> apDev = apDevices[i].Get(0);
    // The correct tracing path for TXOP is to connect to the EdcaTxopN "EdcaTxopN" object in the AP MAC:
    std::ostringstream txopStartPath, txopEndPath;
    txopStartPath << "/NodeList/" << aps[i].Get(0)->GetId()
                  << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/BE_EdcaTxopN/TxopStart";
    txopEndPath << "/NodeList/" << aps[i].Get(0)->GetId()
                << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/BE_EdcaTxopN/TxopEnd";
    Config::Connect(txopStartPath.str(),
      MakeBoundCallback([](Time startTime, uint32_t n, NetStats* s){
        // Store start time in stats for this net id if needed
        (void)startTime; (void)n; (void)s; // Nothing: tracing by intervals, handled in /TxopEnd
      }, i, &netStats[i]));
    Config::Connect(txopEndPath.str(),
      MakeBoundCallback([](Time endTime, uint32_t n, NetStats* s){
        // Trace max TXOP duration: duration = endTime - most recent startTime for this TXOP
        static std::map<uint32_t, Time> lastStartTimes;
        Time start = lastStartTimes[n];
        TxopTrace(start, endTime, n, s);
        lastStartTimes[n] = endTime;
      }, i, &netStats[i]));
  }

  Simulator::Stop(Seconds(simulationTime+0.2));
  Simulator::Run();

  // Compute and print throughput + max TXOP per network
  for (uint32_t i = 0; i < numNets; ++i)
  {
    CalculateThroughput(sinks[i], &netStats[i], simulationTime);
    std::cout << "Network " << configs[i].ssid
              << " (ch " << configs[i].channelNumber << "): "
              << "Throughput = " << netStats[i].throughput << " Mbps, "
              << "Max TXOP duration = " << netStats[i].maxTxop.GetMicroSeconds() << " us"
              << std::endl;
  }

  Simulator::Destroy();
  return 0;
}