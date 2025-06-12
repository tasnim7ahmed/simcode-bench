#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7ThroughputTest");

class ThroughputTester {
public:
  ThroughputTester(uint8_t mcs, uint16_t channelWidth, bool shortGi, uint32_t frequency,
                   bool enableUplinkOfdma, bool enableBsrp, uint32_t nStations,
                   std::string trafficType, uint32_t payloadSize, uint32_t mpduBufferSize,
                   bool enableX1uPhy, bool enableCtPhy);
  void Run(double simulationTime);
  double GetThroughput();

private:
  uint8_t m_mcs;
  uint16_t m_channelWidth;
  bool m_shortGi;
  uint32_t m_frequency;
  bool m_enableUplinkOfdma;
  bool m_enableBsrp;
  uint32_t m_nStations;
  std::string m_trafficType;
  uint32_t m_payloadSize;
  uint32_t m_mpduBufferSize;
  bool m_enableX1uPhy;
  bool m_enableCtPhy;
  double m_throughput;
};

ThroughputTester::ThroughputTester(uint8_t mcs, uint16_t channelWidth, bool shortGi,
                                   uint32_t frequency, bool enableUplinkOfdma,
                                   bool enableBsrp, uint32_t nStations,
                                   std::string trafficType, uint32_t payloadSize,
                                   uint32_t mpduBufferSize, bool enableX1uPhy,
                                   bool enableCtPhy)
    : m_mcs(mcs), m_channelWidth(channelWidth), m_shortGi(shortGi),
      m_frequency(frequency), m_enableUplinkOfdma(enableUplinkOfdma),
      m_enableBsrp(enableBsrp), m_nStations(nStations),
      m_trafficType(trafficType), m_payloadSize(payloadSize),
      m_mpduBufferSize(mpduBufferSize), m_enableX1uPhy(enableX1uPhy),
      m_enableCtPhy(enableCtPhy), m_throughput(0.0) {}

void ThroughputTester::Run(double simulationTime) {
  Time::SetResolution(Time::NS);
  LogComponentEnable("ThroughputTester", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(m_nStations);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211be);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HeMcs" + std::to_string(m_mcs)),
                               "ControlMode", StringValue("HeMcs" + std::to_string(m_mcs)));

  phy.Set("ChannelWidth", UintegerValue(m_channelWidth));
  phy.Set("ShortGuardInterval", BooleanValue(m_shortGi));
  phy.Set("Frequency", UintegerValue(m_frequency));
  if (m_enableX1uPhy) {
    phy.Set("EnableX1uPhy", BooleanValue(true));
  }
  if (m_enableCtPhy) {
    phy.Set("EnableCtPhy", BooleanValue(true));
  }

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("wifi-test")),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("wifi-test")),
              "BeaconInterval", TimeValue(MilliSeconds(100)),
              "EnableUplinkOfdma", BooleanValue(m_enableUplinkOfdma),
              "EnableBsrp", BooleanValue(m_enableBsrp));
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  uint16_t port = 9;

  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (m_trafficType == "TCP") {
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    serverApps = sinkHelper.Install(wifiStaNodes.Get(0));

    BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    clientApps = clientHelper.Install(wifiApNode.Get(0));
  } else if (m_trafficType == "UDP") {
    UdpServerHelper server(port);
    serverApps = server.Install(wifiStaNodes.Get(0));

    UdpClientHelper client(staInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(m_payloadSize));
    clientApps = client.Install(wifiApNode.Get(0));
  }

  serverApps.Start(Seconds(1.0));
  clientApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));
  clientApps.Stop(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  if (m_trafficType == "TCP") {
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));
    uint64_t totalRxBytes = sink->GetTotalRx();
    m_throughput = (totalRxBytes * 8.0) / (simulationTime - 1.0) / 1e6;
  } else if (m_trafficType == "UDP") {
    UdpServer* server = dynamic_cast<UdpServer*>(serverApps.Get(0)->GetObject());
    uint64_t totalRxBytes = server->GetReceived();
    m_throughput = (totalRxBytes * 8.0) / (simulationTime - 1.0) / 1e6;
  }

  double expectedMin = 0.7 * m_channelWidth * 4; // Simplified estimation
  double expectedMax = 1.3 * m_channelWidth * 4;

  if (m_throughput < expectedMin || m_throughput > expectedMax) {
    NS_LOG_ERROR("Throughput out of bounds: " << m_throughput << " Mbps for MCS=" << m_mcs
                                             << ", ChannelWidth=" << m_channelWidth
                                             << ", GI=" << (m_shortGi ? "short" : "long"));
    exit(1);
  }

  Simulator::Destroy();
}

double ThroughputTester::GetThroughput() {
  return m_throughput;
}

int main(int argc, char* argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  std::vector<uint8_t> mcsv = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  std::vector<uint16_t> channelWidths = {20, 40, 80, 160};
  std::vector<bool> giValues = {false, true};
  std::vector<uint32_t> frequencies = {2412, 5180, 5990, 7115}; // 2.4 GHz, 5 GHz, 6 GHz, 6 GHz high
  bool uplinkOfdma = true;
  bool bsrp = true;
  uint32_t nStations = 1;
  uint32_t payloadSize = 1472;
  uint32_t mpduBufferSize = 65536;
  bool x1uPhy = false;
  bool ctPhy = false;
  double simulationTime = 10.0;

  std::cout.precision(2);
  std::cout << std::fixed << "MCS\tChannelWidth\tGI\tThroughput (Mbps)\n";

  for (auto freq : frequencies) {
    for (auto mcs : mcsv) {
      for (auto width : channelWidths) {
        for (auto gi : giValues) {
          ThroughputTester tester(mcs, width, gi, freq, uplinkOfdma, bsrp, nStations,
                                  "UDP", payloadSize, mpduBufferSize, x1uPhy, ctPhy);
          tester.Run(simulationTime);
          std::cout << +mcs << "\t" << width << "\t" << (gi ? "short" : "long")
                    << "\t" << tester.GetThroughput() << "\n";
        }
      }
    }
  }

  return 0;
}