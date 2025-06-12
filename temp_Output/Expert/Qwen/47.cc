#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuse80211ax");

class SpatialReuseExperiment {
public:
  SpatialReuseExperiment();
  void Run(uint32_t channelWidth, double distanceApSta1, double distanceApSta2, double distanceApAp,
           double txPowerAp, double txPowerSta, double ccaEdThreshold,
           double obssPdThreshold, bool enableObssPd, uint32_t packetSize,
           Time interPacketInterval, uint32_t simulationTime, std::string logFile);

private:
  void ResetEvent(std::ofstream& os);
  void LogThroughput(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from);
  void SetupSimulation(uint32_t channelWidth, double distanceApSta1, double distanceApSta2,
                       double distanceApAp, double txPowerAp, double txPowerSta,
                       double ccaEdThreshold, double obssPdThreshold, bool enableObssPd);
  void InstallApplications(Time interval, uint32_t packetSize, uint32_t simulationTime);

  NodeContainer m_apNodes;
  NodeContainer m_staNodes;
  NetDeviceContainer m_apDevices;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterfaces;
  Ipv4InterfaceContainer m_staInterfaces;
  std::vector<double> m_throughputs;
};

SpatialReuseExperiment::SpatialReuseExperiment() {
  m_throughputs.resize(2, 0.0);
}

void
SpatialReuseExperiment::Run(uint32_t channelWidth, double distanceApSta1, double distanceApSta2,
                            double distanceApAp, double txPowerAp, double txPowerSta,
                            double ccaEdThreshold, double obssPdThreshold, bool enableObssPd,
                            uint32_t packetSize, Time interPacketInterval, uint32_t simulationTime,
                            std::string logFile) {
  SetupSimulation(channelWidth, distanceApSta1, distanceApSta2, distanceApAp, txPowerAp, txPowerSta,
                  ccaEdThreshold, obssPdThreshold, enableObssPd);

  InstallApplications(interPacketInterval, packetSize, simulationTime);

  std::ofstream logOs(logFile.c_str());
  if (!logOs.is_open()) {
    NS_LOG_ERROR("Could not open log file: " << logFile);
  } else {
    ResetEvent(logOs);
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Output throughput results
  for (size_t i = 0; i < m_throughputs.size(); ++i) {
    std::cout << "BSS" << i + 1 << " Throughput: " << m_throughputs[i] << " Mbps" << std::endl;
  }

  Simulator::Destroy();
}

void
SpatialReuseExperiment::ResetEvent(std::ofstream& os) {
  os << "OBSS-PD Reset Event Triggered at Simulation Time: " << Simulator::Now().GetSeconds()
     << "s" << std::endl;
}

void
SpatialReuseExperiment::LogThroughput(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet,
                                     const Address &from) {
  static uint64_t totalBytes[2] = {0};
  static Time lastTimestamp[2] = {Seconds(0), Seconds(0)};
  Time now = Simulator::Now();
  int nodeId = from.IsInvalid() ? -1 : NodeList::GetNodeIndex(from.ToNodeId());

  if (nodeId >= 0 && nodeId < 2) {
    totalBytes[nodeId] += packet->GetSize();
    if ((now - lastTimestamp[nodeId]) >= Seconds(1)) {
      double kbps = (totalBytes[nodeId] * 8.0 / (now - lastTimestamp[nodeId]).GetSeconds()) / 1e6;
      m_throughputs[nodeId] = kbps;
      totalBytes[nodeId] = 0;
      lastTimestamp[nodeId] = now;
    }
  }
}

void
SpatialReuseExperiment::SetupSimulation(uint32_t channelWidth, double distanceApSta1,
                                        double distanceApSta2, double distanceApAp,
                                        double txPowerAp, double txPowerSta,
                                        double ccaEdThreshold, double obssPdThreshold,
                                        bool enableObssPd) {
  m_apNodes.Create(2);
  m_staNodes.Create(2);

  YansWifiPhyHelper phy;
  WifiChannelHelper channel = WifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  phy.Set("ChannelWidth", UintegerValue(channelWidth));
  phy.Set("TxPowerStart", DoubleValue(txPowerAp));
  phy.Set("TxPowerEnd", DoubleValue(txPowerAp));
  phy.Set("CcaEdThreshold", DoubleValue(ccaEdThreshold));

  if (enableObssPd) {
    phy.Set("EnableObssPd", BooleanValue(true));
    phy.Set("ObssPdLevel", DoubleValue(obssPdThreshold));
  }

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax);

  Ssid ssid1 = Ssid("ns-3-ssid1");
  Ssid ssid2 = Ssid("ns-3-ssid2");

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
  m_apDevices.Add(wifi.Install(phy, mac, m_apNodes.Get(0)));
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
  m_apDevices.Add(wifi.Install(phy, mac, m_apNodes.Get(1)));

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(0)));
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
  m_staDevices.Add(wifi.Install(phy, mac, m_staNodes.Get(1)));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distanceApAp),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNodes);

  // STA positions relative to APs
  Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator>();
  staPositionAlloc->Add(Vector(distanceApSta1, 0.0, 0.0)); // STA1 near AP1
  staPositionAlloc->Add(Vector(distanceApAp - distanceApSta2, 0.0, 0.0)); // STA2 near AP2
  mobility.SetPositionAllocator(staPositionAlloc);
  mobility.Install(m_staNodes);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterfaces = address.Assign(m_apDevices);
  address.NewNetwork();
  m_staInterfaces = address.Assign(m_staDevices);
}

void
SpatialReuseExperiment::InstallApplications(Time interval, uint32_t packetSize, uint32_t simulationTime) {
  uint16_t port = 9;

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(m_apInterfaces.GetAddress(0), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

  ApplicationContainer app1 = onoff.Install(m_staNodes.Get(0));
  app1.Start(Seconds(0));
  app1.Stop(Seconds(simulationTime));

  onoff = OnOffHelper("ns3::UdpSocketFactory", InetSocketAddress(m_apInterfaces.GetAddress(1), port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

  ApplicationContainer app2 = onoff.Install(m_staNodes.Get(1));
  app2.Start(Seconds(0));
  app2.Stop(Seconds(simulationTime));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp1 = sink.Install(m_apNodes.Get(0));
  sinkApp1.Start(Seconds(0));
  sinkApp1.Stop(Seconds(simulationTime));
  ApplicationContainer sinkApp2 = sink.Install(m_apNodes.Get(1));
  sinkApp2.Start(Seconds(0));
  sinkApp2.Stop(Seconds(simulationTime));

  // Connect log callbacks
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeBoundCallback(&SpatialReuseExperiment::LogThroughput, this),
                  Create<OutputStreamWrapper>("throughput-log.txt", std::ios::out));
}

int
main(int argc, char* argv[]) {
  uint32_t channelWidth = 20;
  double distanceApSta1 = 10.0;
  double distanceApSta2 = 10.0;
  double distanceApAp = 40.0;
  double txPowerAp = 20.0;
  double txPowerSta = 15.0;
  double ccaEdThreshold = -79.0;
  double obssPdThreshold = -82.0;
  bool enableObssPd = true;
  uint32_t packetSize = 1024;
  Time interPacketInterval = MicroSeconds(100);
  uint32_t simulationTime = 10;
  std::string logFile = "spatial-reuse-log.txt";

  CommandLine cmd(__FILE__);
  cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
  cmd.AddValue("distanceApSta1", "Distance between AP1 and STA1 in meters", distanceApSta1);
  cmd.AddValue("distanceApSta2", "Distance between AP2 and STA2 in meters", distanceApSta2);
  cmd.AddValue("distanceApAp", "Distance between AP1 and AP2 in meters", distanceApAp);
  cmd.AddValue("txPowerAp", "Transmit power of APs in dBm", txPowerAp);
  cmd.AddValue("txPowerSta", "Transmit power of STAs in dBm", txPowerSta);
  cmd.AddValue("ccaEdThreshold", "CCA-ED threshold in dBm", ccaEdThreshold);
  cmd.AddValue("obssPdThreshold", "OBSS-PD threshold in dBm", obssPdThreshold);
  cmd.AddValue("enableObssPd", "Enable OBSS-PD spatial reuse", enableObssPd);
  cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue("interPacketInterval", "Inter-packet interval in microseconds", interPacketInterval);
  cmd.AddValue("simulationTime", "Duration of the simulation in seconds", simulationTime);
  cmd.AddValue("logFile", "Output log file for reset events", logFile);
  cmd.Parse(argc, argv);

  SpatialReuseExperiment experiment;
  experiment.Run(channelWidth, distanceApSta1, distanceApSta2, distanceApAp, txPowerAp, txPowerSta,
                 ccaEdThreshold, obssPdThreshold, enableObssPd, packetSize, interPacketInterval,
                 simulationTime, logFile);

  return 0;
}