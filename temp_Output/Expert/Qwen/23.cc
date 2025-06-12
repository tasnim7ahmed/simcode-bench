#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7ThroughputTest");

class WifiThroughputExperiment {
public:
  struct Config {
    uint8_t mcs;
    uint16_t channelWidth;
    bool shortGuardInterval;
    double frequency;
    bool enableUplinkOfdma;
    bool enableBsrp;
    uint32_t nStations;
    std::string trafficType;
    uint32_t payloadSize;
    uint32_t mpduBufferSize;
    bool enableAuxiliaryPhy;
  };

  struct Result {
    double throughputMbps;
    bool valid;
  };

  WifiThroughputExperiment(Config config)
      : m_config(config), m_apNode(CreateObject<Node>()), m_wifiMacHelper(new WifiMacHelper()),
        m_wifiPhyHelper(new HrWifIiPhyHelper()), m_wifiHelper(new WifiHelper()) {}

  ~WifiThroughputExperiment() {
    delete m_wifiMacHelper;
    delete m_wifiPhyHelper;
    delete m_wifiHelper;
  }

  Result Run() {
    NS_LOG_INFO("Running configuration: MCS=" << (uint32_t)m_config.mcs
                                             << ", ChannelWidth=" << m_config.channelWidth
                                             << ", Frequency=" << m_config.frequency / 1e9 << " GHz");

    NodeContainer stationNodes;
    stationNodes.Create(m_config.nStations);

    NodeContainer allNodes = NodeContainer(m_apNode, stationNodes);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HeMcs" + std::to_string(m_config.mcs)),
                                 "ControlMode", StringValue("HeMcs0"));

    // Configure PHY
    phy.Set("ChannelWidth", UintegerValue(m_config.channelWidth));
    phy.Set("ShortGuardInterval", BooleanValue(m_config.shortGuardInterval));
    phy.Set("EnableUplinkOfdma", BooleanValue(m_config.enableUplinkOfdma));
    phy.Set("EnableBsrp", BooleanValue(m_config.enableBsrp));
    phy.Set("EnableAuxiliaryPhy", BooleanValue(m_config.enableAuxiliaryPhy));

    // Set frequency based on band
    if (m_config.frequency == 2.4e9 || m_config.frequency == 5e9 || m_config.frequency == 6e9) {
      phy.Set("Frequency", DoubleValue(m_config.frequency));
    } else {
      NS_FATAL_ERROR("Invalid frequency specified");
    }

    // Setup MAC
    Ssid ssid = Ssid("wifi-test");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, stationNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, m_apNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Traffic setup
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    if (m_config.trafficType == "TCP") {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
      serverApps = sink.Install(stationNodes);

      OnOffHelper onoff("ns3::TcpSocketFactory", Address());
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(m_config.payloadSize));

      for (uint32_t i = 0; i < stationNodes.GetN(); ++i) {
        AddressValue remoteAddress(InetSocketAddress(staInterfaces.GetAddress(i), 9));
        onoff.SetAttribute("Remote", remoteAddress);
        clientApps.Add(onoff.Install(m_apNode));
      }
    } else if (m_config.trafficType == "UDP") {
      UdpServerHelper server;
      serverApps = server.Install(stationNodes);

      UdpClientHelper client;
      client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(m_config.payloadSize));

      for (uint32_t i = 0; i < stationNodes.GetN(); ++i) {
        client.SetAttribute("RemoteAddress", AddressValue(staInterfaces.GetAddress(i)));
        clientApps.Add(client.Install(m_apNode));
      }
    } else {
      NS_FATAL_ERROR("Unsupported traffic type: " << m_config.trafficType);
    }

    serverApps.Start(Seconds(1.0));
    clientApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.5));
    Simulator::Run();

    uint64_t totalRxBytes = 0;
    for (uint32_t i = 0; i < stationNodes.GetN(); ++i) {
      Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(i));
      totalRxBytes += sink->GetTotalRx();
    }

    double throughput = (totalRxBytes * 8.0) / (9.0 * 1e6); // Mbps
    Result result;
    result.throughputMbps = throughput;

    // Validate against expected range (simplified model)
    double expectedBandwidthPerStation = CalculateExpectedBandwidth();
    double minExpected = expectedBandwidthPerStation * 0.7 * m_config.nStations;
    double maxExpected = expectedBandwidthPerStation * 1.1 * m_config.nStations;

    result.valid = (throughput >= minExpected && throughput <= maxExpected);

    if (!result.valid) {
      NS_LOG_ERROR("Throughput out of expected bounds: " << throughput << " Mbps (expected between " << minExpected << " and " << maxExpected << ")");
    }

    Simulator::Destroy();
    return result;
  }

private:
  double CalculateExpectedBandwidth() {
    // Simplified bandwidth calculation for example purposes
    // Real implementation would use actual HE PHY rates
    double baseRate = 100.0; // Mbps per stream
    double giFactor = m_config.shortGuardInterval ? 1.11 : 1.0;
    double widthFactor = static_cast<double>(m_config.channelWidth) / 20.0;
    return baseRate * (m_config.mcs + 1) * widthFactor * giFactor;
  }

  Config m_config;
  Ptr<Node> m_apNode;
  WifiMacHelper* m_wifiMacHelper;
  HrWifIiPhyHelper* m_wifiPhyHelper;
  WifiHelper* m_wifiHelper;
};

int main(int argc, char* argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  std::vector<uint8_t> mcsv = {0, 4, 8, 11};
  std::vector<uint16_t> widths = {20, 40, 80, 160};
  std::vector<bool> gis = {false, true};
  std::vector<double> frequencies = {2.4e9, 5e9, 6e9};
  std::vector<std::string> trafficTypes = {"TCP", "UDP"};

  std::vector<std::tuple<uint8_t, uint16_t, bool, double, std::string>> testCases;

  for (auto mcs : mcsv) {
    for (auto width : widths) {
      for (auto gi : gis) {
        for (auto freq : frequencies) {
          for (auto& traffic : trafficTypes) {
            testCases.emplace_back(mcs, width, gi, freq, traffic);
          }
        }
      }
    }
  }

  std::cout << "MCS\tWidth(MHz)\tGI(ns)\tFreq(GHz)\tTraffic\tThroughput(Mbps)\tValid\n";

  bool overallSuccess = true;

  for (auto& [mcs, width, gi, freq, traffic] : testCases) {
    WifiThroughputExperiment::Config config;
    config.mcs = mcs;
    config.channelWidth = width;
    config.shortGuardInterval = gi;
    config.frequency = freq;
    config.enableUplinkOfdma = true;
    config.enableBsrp = true;
    config.nStations = 5;
    config.trafficType = traffic;
    config.payloadSize = 1472;
    config.mpduBufferSize = 65536;
    config.enableAuxiliaryPhy = false;

    WifiThroughputExperiment experiment(config);
    auto result = experiment.Run();

    std::string giStr = gi ? "800" : "1600";
    std::cout << (uint32_t)mcs << "\t" << width << "\t\t" << giStr << "\t"
              << freq / 1e9 << "\t\t" << traffic << "\t" << result.throughputMbps
              << "\t\t" << (result.valid ? "Yes" : "No") << "\n";

    if (!result.valid) {
      overallSuccess = false;
    }
  }

  if (!overallSuccess) {
    NS_LOG_ERROR("One or more test cases failed throughput validation.");
    return 1;
  }

  return 0;
}