#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuse80211ax");

class SpatialReuseExperiment {
public:
  SpatialReuseExperiment();
  void Setup(bool enableObssPd, double distanceApSta1, double distanceApSta2, double apApDistance,
             uint32_t channelWidth, double txPowerAp, double txPowerSta,
             double ccaEdThreshold, double obssPdThreshold,
             Time interval, uint32_t packetSize, Time simTime);
  void LogResetEvent(std::string filename);
  void OutputThroughput();

private:
  Ptr<YansWifiChannel> CreateChannel(double lossRefDistance = 1.0, double lossExponent = 3.0);
  void SetupNodes();
  void SetupDevices();
  void SetupApplications();
  void CalculateThroughput();

  NodeContainer m_apNodes;
  NodeContainer m_staNodes;
  NetDeviceContainer m_apDevices;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterfaces;
  Ipv4InterfaceContainer m_staInterfaces;

  bool m_enableObssPd;
  uint32_t m_channelWidth;
  double m_txPowerAp;
  double m_txPowerSta;
  double m_ccaEdThreshold;
  double m_obssPdThreshold;
  double m_distanceApSta1;
  double m_distanceApSta2;
  double m_apApDistance;
  Time m_interval;
  uint32_t m_packetSize;
  Time m_simTime;

  std::map<Ptr<Socket>, uint64_t> m_receivedBytes;
};

SpatialReuseExperiment::SpatialReuseExperiment() {
  m_apNodes.Create(2);
  m_staNodes.Create(2);
}

void
SpatialReuseExperiment::Setup(bool enableObssPd, double distanceApSta1, double distanceApSta2, double apApDistance,
                              uint32_t channelWidth, double txPowerAp, double txPowerSta,
                              double ccaEdThreshold, double obssPdThreshold,
                              Time interval, uint32_t packetSize, Time simTime) {
  m_enableObssPd = enableObssPd;
  m_distanceApSta1 = distanceApSta1;
  m_distanceApSta2 = distanceApSta2;
  m_apApDistance = apApDistance;
  m_channelWidth = channelWidth;
  m_txPowerAp = txPowerAp;
  m_txPowerSta = txPowerSta;
  m_ccaEdThreshold = ccaEdThreshold;
  m_obssPdThreshold = obssPdThreshold;
  m_interval = interval;
  m_packetSize = packetSize;
  m_simTime = simTime;

  SetupNodes();
  SetupDevices();
  SetupApplications();
}

void
SpatialReuseExperiment::SetupNodes() {
  InternetStackHelper stack;
  stack.Install(m_apNodes);
  stack.Install(m_staNodes);

  // Mobility: APs and STAs in fixed positions
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP1
  positionAlloc->Add(Vector(m_apApDistance, 0.0, 0.0)); // AP2
  positionAlloc->Add(Vector(m_distanceApSta1, 0.0, 0.0)); // STA1
  positionAlloc->Add(Vector(m_apApDistance - m_distanceApSta2, 0.0, 0.0)); // STA2

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(NodeContainer(m_apNodes, m_staNodes));
}

void
SpatialReuseExperiment::SetupDevices() {
  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ax);

  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(Create<SpatialReuseExperiment>(this, &SpatialReuseExperiment::CreateChannel));

  // Configure OBSS-PD and CCA-ED thresholds
  wifiPhy.Set("CcaEdThreshold", DoubleValue(m_ccaEdThreshold));
  if (m_enableObssPd) {
    wifiPhy.Set("EnableObssPd", BooleanValue(true));
    wifiPhy.Set("ObssPdLevel", DoubleValue(m_obssPdThreshold));
  } else {
    wifiPhy.Set("EnableObssPd", BooleanValue(false));
  }

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HeMcs0"),
                               "ControlMode", StringValue("HeMcs0"));

  // BSS1: AP1 and STA1
  Ssid ssid1 = Ssid("BSS1");
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid1),
                  "BeaconInterval", TimeValue(MilliSeconds(100)));
  m_apDevices.Add(wifi.Install(wifiPhy, wifiMac, m_apNodes.Get(0)));

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid1),
                  "ActiveProbing", BooleanValue(false));
  m_staDevices.Add(wifi.Install(wifiPhy, wifiMac, m_staNodes.Get(0)));

  // BSS2: AP2 and STA2
  Ssid ssid2 = Ssid("BSS2");
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid2),
                  "BeaconInterval", TimeValue(MilliSeconds(100)));
  m_apDevices.Add(wifi.Install(wifiPhy, wifiMac, m_apNodes.Get(1)));

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid2),
                  "ActiveProbing", BooleanValue(false));
  m_staDevices.Add(wifi.Install(wifiPhy, wifiMac, m_staNodes.Get(1)));

  // Set channel width
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(m_channelWidth));

  // Set TX power
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(m_txPowerAp));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(m_txPowerAp));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(m_txPowerSta));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(m_txPowerSta));

  // IPv4 setup
  InternetStackHelper stack;
  Ipv4AddressHelper address;

  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterfaces = address.Assign(m_apDevices);
  address.SetBase("192.168.2.0", "255.255.255.0");
  m_staInterfaces = address.Assign(m_staDevices);
}

Ptr<YansWifiChannel>
SpatialReuseExperiment::CreateChannel(double lossRefDistance, double lossExponent) {
  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
  Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
  lossModel->SetReference(lossRefDistance, 46.6777); // Free space at 5 GHz
  lossModel->SetExponent(lossExponent);
  channel->AddPropagationLossModel(lossModel);

  Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
  channel->SetPropagationDelayModel(delayModel);
  return channel;
}

void
SpatialReuseExperiment::SetupApplications() {
  // UDP traffic from STA1 to AP1
  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(m_apInterfaces.GetAddress(0), port)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(m_interval.GetSeconds()) + "]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(m_packetSize));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

  ApplicationContainer sta1App = onoff.Install(m_staNodes.Get(0));
  sta1App.Start(Seconds(0.0));
  sta1App.Stop(m_simTime);

  // UDP traffic from STA2 to AP2
  OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(m_apInterfaces.GetAddress(1), port)));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(m_interval.GetSeconds()) + "]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff2.SetAttribute("PacketSize", UintegerValue(m_packetSize));
  onoff2.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

  ApplicationContainer sta2App = onoff2.Install(m_staNodes.Get(1));
  sta2App.Start(Seconds(0.0));
  sta2App.Stop(m_simTime);

  // Sink applications on APs
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer ap1Sink = sink.Install(m_apNodes.Get(0));
  ApplicationContainer ap2Sink = sink.Install(m_apNodes.Get(1));
  ap1Sink.Start(Seconds(0.0));
  ap2Sink.Start(Seconds(0.0));
  ap1Sink.Stop(m_simTime);
  ap2Sink.Stop(m_simTime);

  // Throughput tracking
  for (uint32_t i = 0; i < ap1Sink.GetN(); ++i) {
    Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(ap1Sink.Get(i));
    m_receivedBytes[sinkApp->GetSocket()] = 0;
    sinkApp->TraceConnectWithoutContext("Rx", MakeCallback(&SpatialReuseExperiment::CalculateThroughput, this));
  }
  for (uint32_t i = 0; i < ap2Sink.GetN(); ++i) {
    Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(ap2Sink.Get(i));
    m_receivedBytes[sinkApp->GetSocket()] = 0;
    sinkApp->TraceConnectWithoutContext("Rx", MakeCallback(&SpatialReuseExperiment::CalculateThroughput, this));
  }
}

void
SpatialReuseExperiment::CalculateThroughput(Ptr<Socket> socket, Ptr<const Packet> packet) {
  m_receivedBytes[socket] += packet->GetSize();
}

void
SpatialReuseExperiment::OutputThroughput() {
  Time endTime = m_simTime;
  double durationSeconds = endTime.GetSeconds();
  double throughput1 = (static_cast<double>(m_receivedBytes.begin()->second) * 8) / (durationSeconds * 1e6);
  double throughput2 = (static_cast<double>((++m_receivedBytes.begin())->second) * 8) / (durationSeconds * 1e6);
  std::cout.precision(4);
  std::cout << "Throughput BSS1: " << throughput1 << " Mbps" << std::endl;
  std::cout << "Throughput BSS2: " << throughput2 << " Mbps" << std::endl;
}

void
SpatialReuseExperiment::LogResetEvent(std::string filename) {
  std::ofstream logFile(filename, std::ios::app);
  if (!logFile.is_open()) {
    NS_LOG_WARN("Could not open log file.");
    return;
  }
  logFile << "Simulation with OBSS-PD " << (m_enableObssPd ? "enabled" : "disabled")
          << ", Channel Width: " << m_channelWidth << " MHz"
          << ", Distance AP-STA1: " << m_distanceApSta1 << " m"
          << ", Distance AP-STA2: " << m_distanceApSta2 << " m"
          << ", AP-AP Distance: " << m_apApDistance << " m"
          << ", TX Power AP: " << m_txPowerAp << " dBm"
          << ", TX Power STA: " << m_txPowerSta << " dBm"
          << ", CCA-ED Threshold: " << m_ccaEdThreshold << " dBm"
          << ", OBSS-PD Threshold: " << m_obssPdThreshold << " dBm"
          << std::endl;
  logFile.close();
}

int
main(int argc, char *argv[]) {
  bool enableObssPd = true;
  double d1 = 10.0, d2 = 10.0, d3 = 20.0;
  double txPowerAp = 20.0, txPowerSta = 15.0;
  double ccaEdThreshold = -72.0, obssPdThreshold = -82.0;
  uint32_t channelWidth = 20;
  Time interval = Seconds(0.1);
  uint32_t packetSize = 1024;
  Time simTime = Seconds(10.0);
  std::string logFilename = "spatial_reuse_log.txt";

  CommandLine cmd(__FILE__);
  cmd.AddValue("enableObssPd", "Enable or disable OBSS-PD spatial reuse", enableObssPd);
  cmd.AddValue("distanceApSta1", "Distance between AP1 and STA1 (meters)", d1);
  cmd.AddValue("distanceApSta2", "Distance between AP2 and STA2 (meters)", d2);
  cmd.AddValue("apApDistance", "Distance between AP1 and AP2 (meters)", d3);
  cmd.AddValue("channelWidth", "WiFi channel width in MHz (20, 40, 80)", channelWidth);
  cmd.AddValue("txPowerAp", "Transmit power of APs in dBm", txPowerAp);
  cmd.AddValue("txPowerSta", "Transmit power of STAs in dBm", txPowerSta);
  cmd.AddValue("ccaEdThreshold", "CCA-ED threshold in dBm", ccaEdThreshold);
  cmd.AddValue("obssPdThreshold", "OBSS-PD threshold in dBm", obssPdThreshold);
  cmd.AddValue("interval", "Traffic interval between packets", interval);
  cmd.AddValue("packetSize", "Size of transmitted packets", packetSize);
  cmd.AddValue("simTime", "Total simulation time", simTime);
  cmd.AddValue("logFile", "File to log reset events", logFilename);
  cmd.Parse(argc, argv);

  SpatialReuseExperiment experiment;
  experiment.Setup(enableObssPd, d1, d2, d3, channelWidth, txPowerAp, txPowerSta,
                   ccaEdThreshold, obssPdThreshold, interval, packetSize, simTime);
  experiment.LogResetEvent(logFilename);
  Simulator::Stop(simTime);
  Simulator::Run();
  experiment.OutputThroughput();
  Simulator::Destroy();
  return 0;
}