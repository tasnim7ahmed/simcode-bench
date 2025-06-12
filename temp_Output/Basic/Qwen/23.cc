#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7ThroughputTest");

class Wifi7ThroughputExperiment
{
public:
  struct TestConfig
  {
    uint8_t mcs;
    uint16_t channelWidth;
    bool shortGuardInterval;
    double expectedMinTputMbps;
    double expectedMaxTputMbps;
  };

  Wifi7ThroughputExperiment();
  void Run(std::vector<TestConfig> configs,
           uint32_t nStations,
           Time simulationTime,
           std::string trafficType,
           uint32_t payloadSize,
           uint32_t mpduBufferSize,
           bool enableDlOfdma,
           bool enableBsrp,
           uint32_t auxPhyPreamble);

private:
  void SetupSimulation(TestConfig config,
                       uint32_t nStations,
                       Time simulationTime,
                       std::string trafficType,
                       uint32_t payloadSize,
                       uint32_t mpduBufferSize,
                       bool enableDlOfdma,
                       bool enableBsrp,
                       uint32_t auxPhyPreamble);
  void OnTxPacket(Ptr<const Packet>);
  void OnRxPacket(Ptr<const Packet>, const Address &);
  void ReportResult(TestConfig config, double throughputMbps);

  uint64_t m_totalRxBytes;
};

Wifi7ThroughputExperiment::Wifi7ThroughputExperiment()
    : m_totalRxBytes(0)
{
}

void Wifi7ThroughputExperiment::Run(std::vector<TestConfig> configs,
                                   uint32_t nStations,
                                   Time simulationTime,
                                   std::string trafficType,
                                   uint32_t payloadSize,
                                   uint32_t mpduBufferSize,
                                   bool enableDlOfdma,
                                   bool enableBsrp,
                                   uint32_t auxPhyPreamble)
{
  for (auto &config : configs)
  {
    NS_LOG_INFO("Running test with MCS=" << +config.mcs
                                        << ", Channel Width=" << config.channelWidth
                                        << " MHz, GI=" << (config.shortGuardInterval ? "Short" : "Long"));
    SetupSimulation(config, nStations, simulationTime, trafficType, payloadSize, mpduBufferSize,
                    enableDlOfdma, enableBsrp, auxPhyPreamble);
  }
}

void Wifi7ThroughputExperiment::SetupSimulation(TestConfig config,
                                                uint32_t nStations,
                                                Time simulationTime,
                                                std::string trafficType,
                                                uint32_t payloadSize,
                                                uint32_t mpduBufferSize,
                                                bool enableDlOfdma,
                                                bool enableBsrp,
                                                uint32_t auxPhyPreamble)
{
  m_totalRxBytes = 0;

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nStations);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211be);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HeMcs" + std::to_string(config.mcs)),
                               "ControlMode", StringValue("HeMcs" + std::to_string(config.mcs)));

  phy.Set("ChannelWidth", UintegerValue(config.channelWidth));
  phy.Set("ShortGuardIntervalSupported", BooleanValue(config.shortGuardInterval));
  phy.Set("PreambleDetectionThreshold", UintegerValue(auxPhyPreamble));

  mac.SetType("ns3::StaWifiMac");
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac");
  NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(wifiApNode);
  mobility.Install(wifiStaNodes);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode",
              StringValue("HeMcs" + std::to_string(config.mcs)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ControlMode",
              StringValue("HeMcs" + std::to_string(config.mcs)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth",
              UintegerValue(config.channelWidth));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported",
              BooleanValue(config.shortGuardInterval));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PreambleDetectionThreshold",
              UintegerValue(auxPhyPreamble));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize",
              UintegerValue(mpduBufferSize));

  if (enableDlOfdma)
  {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/UseGreenfieldProtection",
                BooleanValue(true));
  }

  if (enableBsrp)
  {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaParametersList/0/Queue/MinCw",
                UintegerValue(15));
  }

  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (trafficType == "TCP")
  {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    serverApps.Add(sink.Install(wifiApNode.Get(0)));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simulationTime);

    for (uint32_t i = 0; i < nStations; ++i)
    {
      Ptr<Node> node = wifiStaNodes.Get(i);
      InetSocketAddress remote(apInterface.GetAddress(0), 9);
      OnOffHelper onoff("ns3::TcpSocketFactory", remote);
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      clientApps.Add(onoff.Install(node));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime - Seconds(0.1));
  }
  else if (trafficType == "UDP")
  {
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    serverApps.Add(sink.Install(wifiApNode.Get(0)));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simulationTime);

    for (uint32_t i = 0; i < nStations; ++i)
    {
      Ptr<Node> node = wifiStaNodes.Get(i);
      InetSocketAddress remote(apInterface.GetAddress(0), 9);
      OnOffHelper onoff("ns3::UdpSocketFactory", remote);
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
      onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
      clientApps.Add(onoff.Install(node));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime - Seconds(0.1));
  }

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&Wifi7ThroughputExperiment::OnRxPacket, this));

  Simulator::Stop(simulationTime);
  Simulator::Run();

  double throughputMbps = (m_totalRxBytes * 8.0) / (simulationTime.GetSeconds() * 1e6);
  ReportResult(config, throughputMbps);

  Simulator::Destroy();
}

void Wifi7ThroughputExperiment::OnRxPacket(Ptr<const Packet> packet, const Address &from)
{
  m_totalRxBytes += packet->GetSize();
}

void Wifi7ThroughputExperiment::ReportResult(TestConfig config, double throughputMbps)
{
  NS_LOG_UNCOND("MCS: " << +config.mcs
                        << " | Channel Width: " << config.channelWidth
                        << " MHz | GI: " << (config.shortGuardInterval ? "Short" : "Long")
                        << " | Throughput: " << throughputMbps << " Mbps");

  if (throughputMbps < config.expectedMinTputMbps || throughputMbps > config.expectedMaxTputMbps)
  {
    NS_LOG_ERROR("Throughput out of bounds! Expected [" << config.expectedMinTputMbps
                                                       << "-" << config.expectedMaxTputMbps
                                                       << "] Mbps, got " << throughputMbps << " Mbps.");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[])
{
  uint32_t nStations = 5;
  double simulationTimeSecs = 10;
  std::string trafficType = "UDP";
  uint32_t payloadSize = 1472;
  uint32_t mpduBufferSize = 65536;
  bool enableDlOfdma = true;
  bool enableBsrp = true;
  uint32_t auxPhyPreamble = 0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nStations", "Number of stations", nStations);
  cmd.AddValue("time", "Simulation time in seconds", simulationTimeSecs);
  cmd.AddValue("trafficType", "Traffic type: TCP or UDP", trafficType);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
  cmd.AddValue("enableDlOfdma", "Enable Downlink OFDMA", enableDlOfdma);
  cmd.AddValue("enableBsrp", "Enable BSRP", enableBsrp);
  cmd.AddValue("auxPhyPreamble", "Auxiliary PHY preamble detection threshold", auxPhyPreamble);
  cmd.Parse(argc, argv);

  Time simulationTime = Seconds(simulationTimeSecs);

  std::vector<Wifi7ThroughputExperiment::TestConfig> configs = {
      {0, 20, false, 10.0, 20.0},
      {4, 40, true, 40.0, 60.0},
      {7, 80, true, 80.0, 120.0},
      {11, 160, true, 150.0, 250.0}};

  Wifi7ThroughputExperiment experiment;
  experiment.Run(configs, nStations, simulationTime, trafficType, payloadSize, mpduBufferSize,
                 enableDlOfdma, enableBsrp, auxPhyPreamble);

  return 0;
}