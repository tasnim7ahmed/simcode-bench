#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuse80211ax");

class SpatialReuseExperiment {
public:
  SpatialReuseExperiment();
  void Run(uint32_t channelWidth, double distanceApSta1, double distanceApSta2, double distanceApAp,
           double txPowerAp1, double txPowerSta1, double txPowerAp2, double txPowerSta2,
           double ccaEdThresholdAp1, double ccaEdThresholdSta1, double ccaEdThresholdAp2, double ccaEdThresholdSta2,
           double obssPdThresholdAp1, double obssPdThresholdSta1, double obssPdThresholdAp2, double obssPdThresholdSta2,
           bool enableObssPd, uint32_t packetSize, Time interval, Time simulationTime, std::string logFile);

private:
  void SetupNetwork(uint32_t channelWidth, double distanceApSta1, double distanceApSta2, double distanceApAp);
  void SetupDevices(double txPowerAp1, double txPowerSta1, double txPowerAp2, double txPowerSta2,
                    double ccaEdThresholdAp1, double ccaEdThresholdSta1, double ccaEdThresholdAp2, double ccaEdThresholdSta2,
                    double obssPdThresholdAp1, double obssPdThresholdSta1, double obssPdThresholdAp2, double obssPdThresholdSta2,
                    bool enableObssPd);
  void InstallApplications(Time interval, uint32_t packetSize, Time simulationTime);
  void EnableTraces();
  void LogResetEvents(std::string filename);

  NodeContainer m_apNodes;
  NodeContainer m_staNodes;
  NetDeviceContainer m_apDevices;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterfaces;
  Ipv4InterfaceContainer m_staInterfaces;
  std::ofstream m_logStream;
};

SpatialReuseExperiment::SpatialReuseExperiment() {
  m_apNodes.Create(2);
  m_staNodes.Create(2);
}

void
SpatialReuseExperiment::Run(uint32_t channelWidth, double distanceApSta1, double distanceApSta2, double distanceApAp,
                            double txPowerAp1, double txPowerSta1, double txPowerAp2, double txPowerSta2,
                            double ccaEdThresholdAp1, double ccaEdThresholdSta1, double ccaEdThresholdAp2, double ccaEdThresholdSta2,
                            double obssPdThresholdAp1, double obssPdThresholdSta1, double obssPdThresholdAp2, double obssPdThresholdSta2,
                            bool enableObssPd, uint32_t packetSize, Time interval, Time simulationTime, std::string logFile) {
  SetupNetwork(channelWidth, distanceApSta1, distanceApSta2, distanceApAp);
  SetupDevices(txPowerAp1, txPowerSta1, txPowerAp2, txPowerSta2,
               ccaEdThresholdAp1, ccaEdThresholdSta1, ccaEdThresholdAp2, ccaEdThresholdSta2,
               obssPdThresholdAp1, obssPdThresholdSta1, obssPdThresholdAp2, obssPdThresholdSta2,
               enableObssPd);
  InstallApplications(interval, packetSize, simulationTime);
  EnableTraces();
  LogResetEvents(logFile);

  Simulator::Stop(simulationTime);
  Simulator::Run();
  Simulator::Destroy();
}

void
SpatialReuseExperiment::SetupNetwork(uint32_t channelWidth, double distanceApSta1, double distanceApSta2, double distanceApAp) {
  // Mobility: APs and STAs placed in line
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP1
  positionAlloc->Add(Vector(distanceApAp, 0.0, 0.0)); // AP2
  positionAlloc->Add(Vector(distanceApSta1, 0.0, 0.0)); // STA1 relative to AP1
  positionAlloc->Add(Vector(distanceApAp + distanceApSta2, 0.0, 0.0)); // STA2 relative to AP2

  MobilityHelper mobility;
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNodes);
  mobility.Install(m_staNodes);

  // Channel configuration
  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());
  phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));
  phyHelper.Set("TxPowerStart", DoubleValue(0.0));
  phyHelper.Set("TxPowerEnd", DoubleValue(0.0));
}

void
SpatialReuseExperiment::SetupDevices(double txPowerAp1, double txPowerSta1, double txPowerAp2, double txPowerSta2,
                                    double ccaEdThresholdAp1, double ccaEdThresholdSta1, double ccaEdThresholdAp2, double ccaEdThresholdSta2,
                                    double obssPdThresholdAp1, double obssPdThresholdSta1, double obssPdThresholdAp2, double obssPdThresholdSta2,
                                    bool enableObssPd) {
  WifiMacHelper macHelper;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211ax);
  wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HeMcs0"), "ControlMode", StringValue("HeMcs0"));

  // BSS 1
  Ssid ssid1 = Ssid("BSS1");
  macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices1 = wifiHelper.Install(YansWifiPhyHelper::Default(), macHelper, m_staNodes.Get(0));
  macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
  NetDeviceContainer apDevices1 = wifiHelper.Install(YansWifiPhyHelper::Default(), macHelper, m_apNodes.Get(0));

  // BSS 2
  Ssid ssid2 = Ssid("BSS2");
  macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices2 = wifiHelper.Install(YansWifiPhyHelper::Default(), macHelper, m_staNodes.Get(1));
  macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
  NetDeviceContainer apDevices2 = wifiHelper.Install(YansWifiPhyHelper::Default(), macHelper, m_apNodes.Get(1));

  m_staDevices.Add(staDevices1);
  m_staDevices.Add(staDevices2);
  m_apDevices.Add(apDevices1);
  m_apDevices.Add(apDevices2);

  // Configure transmit power and CCA
  for (size_t i = 0; i < m_apDevices.GetN(); ++i) {
    auto dev = DynamicCast<WifiNetDevice>(m_apDevices.Get(i));
    if (i == 0) {
      dev->GetPhy()->SetTxPowerLevel(0); // Use default TX power control
      dev->GetPhy()->SetAttribute("TxPower", DoubleValue(txPowerAp1));
      dev->GetPhy()->SetAttribute("CcaEdThreshold", DoubleValue(ccaEdThresholdAp1));
    } else {
      dev->GetPhy()->SetTxPowerLevel(0);
      dev->GetPhy()->SetAttribute("TxPower", DoubleValue(txPowerAp2));
      dev->GetPhy()->SetAttribute("CcaEdThreshold", DoubleValue(ccaEdThresholdAp2));
    }
    if (enableObssPd) {
      dev->GetPhy()->SetAttribute("ObssPdLevel", DoubleValue(obssPdThresholdAp1 + (i > 0 ? obssPdThresholdAp2 - obssPdThresholdAp1 : 0)));
    }
  }

  for (size_t i = 0; i < m_staDevices.GetN(); ++i) {
    auto dev = DynamicCast<WifiNetDevice>(m_staDevices.Get(i));
    dev->GetPhy()->SetTxPowerLevel(0);
    if (i == 0) {
      dev->GetPhy()->SetAttribute("TxPower", DoubleValue(txPowerSta1));
      dev->GetPhy()->SetAttribute("CcaEdThreshold", DoubleValue(ccaEdThresholdSta1));
    } else {
      dev->GetPhy()->SetAttribute("TxPower", DoubleValue(txPowerSta2));
      dev->GetPhy()->SetAttribute("CcaEdThreshold", DoubleValue(ccaEdThresholdSta2));
    }
    if (enableObssPd) {
      dev->GetPhy()->SetAttribute("ObssPdLevel", DoubleValue(obssPdThresholdSta1 + (i > 0 ? obssPdThresholdSta2 - obssPdThresholdSta1 : 0)));
    }
  }
}

void
SpatialReuseExperiment::InstallApplications(Time interval, uint32_t packetSize, Time simulationTime) {
  InternetStackHelper internet;
  internet.Install(NodeContainer(m_apNodes, m_staNodes));

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterfaces = address.Assign(m_apDevices);
  m_staInterfaces = address.Assign(m_staDevices);

  // UDP Echo Server on AP1 and AP2
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(m_apNodes);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(simulationTime);

  // UDP Clients from STA1 and STA2
  UdpEchoClientHelper clientHelper1(m_apInterfaces.GetAddress(0), 9);
  clientHelper1.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  clientHelper1.SetAttribute("Interval", TimeValue(interval));
  clientHelper1.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp1 = clientHelper1.Install(m_staNodes.Get(0));
  clientApp1.Start(Seconds(1.0));
  clientApp1.Stop(simulationTime);

  UdpEchoClientHelper clientHelper2(m_apInterfaces.GetAddress(1), 9);
  clientHelper2.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  clientHelper2.SetAttribute("Interval", TimeValue(interval));
  clientHelper2.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApp2 = clientHelper2.Install(m_staNodes.Get(1));
  clientApp2.Start(Seconds(1.0));
  clientApp2.Stop(simulationTime);
}

void
SpatialReuseExperiment::EnableTraces() {
  AsciiTraceHelper asciiTraceHelper;
  m_logStream = asciiTraceHelper.CreateFileStream("spatial-reuse.tr");
  m_logStream << "Simulation started with spatial reuse experiment." << std::endl;
}

void
SpatialReuseExperiment::LogResetEvents(std::string filename) {
  std::ofstream file(filename);
  file << "OBSS-PD Reset events:" << std::endl;
  file.close();
}

int
main(int argc, char *argv[]) {
  uint32_t channelWidth = 20;
  double d1 = 10.0, d2 = 10.0, d3 = 20.0;
  double txPowerAp1 = 20.0, txPowerSta1 = 15.0;
  double txPowerAp2 = 20.0, txPowerSta2 = 15.0;
  double ccaEdThresholdAp1 = -79.0, ccaEdThresholdSta1 = -79.0;
  double ccaEdThresholdAp2 = -79.0, ccaEdThresholdSta2 = -79.0;
  double obssPdThresholdAp1 = -82.0, obssPdThresholdSta1 = -82.0;
  double obssPdThresholdAp2 = -82.0, obssPdThresholdSta2 = -82.0;
  bool enableObssPd = true;
  uint32_t packetSize = 1024;
  Time interval = MilliSeconds(100);
  Time simulationTime = Seconds(10);
  std::string logFile = "reset_events.log";

  CommandLine cmd(__FILE__);
  cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
  cmd.AddValue("distanceApSta1", "Distance between AP1 and STA1 (meters)", d1);
  cmd.AddValue("distanceApSta2", "Distance between AP2 and STA2 (meters)", d2);
  cmd.AddValue("distanceApAp", "Distance between AP1 and AP2 (meters)", d3);
  cmd.AddValue("txPowerAp1", "Transmit power of AP1 (dBm)", txPowerAp1);
  cmd.AddValue("txPowerSta1", "Transmit power of STA1 (dBm)", txPowerSta1);
  cmd.AddValue("txPowerAp2", "Transmit power of AP2 (dBm)", txPowerAp2);
  cmd.AddValue("txPowerSta2", "Transmit power of STA2 (dBm)", txPowerSta2);
  cmd.AddValue("ccaEdThresholdAp1", "CCA-ED threshold for AP1 (dBm)", ccaEdThresholdAp1);
  cmd.AddValue("ccaEdThresholdSta1", "CCA-ED threshold for STA1 (dBm)", ccaEdThresholdSta1);
  cmd.AddValue("ccaEdThresholdAp2", "CCA-ED threshold for AP2 (dBm)", ccaEdThresholdAp2);
  cmd.AddValue("ccaEdThresholdSta2", "CCA-ED threshold for STA2 (dBm)", ccaEdThresholdSta2);
  cmd.AddValue("obssPdThresholdAp1", "OBSS-PD threshold for AP1 (dBm)", obssPdThresholdAp1);
  cmd.AddValue("obssPdThresholdSta1", "OBSS-PD threshold for STA1 (dBm)", obssPdThresholdSta1);
  cmd.AddValue("obssPdThresholdAp2", "OBSS-PD threshold for AP2 (dBm)", obssPdThresholdAp2);
  cmd.AddValue("obssPdThresholdSta2", "OBSS-PD threshold for STA2 (dBm)", obssPdThresholdSta2);
  cmd.AddValue("enableObssPd", "Enable OBSS-PD spatial reuse", enableObssPd);
  cmd.AddValue("packetSize", "Size of packets (bytes)", packetSize);
  cmd.AddValue("interval", "Interval between packets", interval);
  cmd.AddValue("simulationTime", "Duration of simulation", simulationTime);
  cmd.AddValue("logFile", "Output log file for reset events", logFile);
  cmd.Parse(argc, argv);

  SpatialReuseExperiment experiment;
  experiment.Run(channelWidth, d1, d2, d3,
                 txPowerAp1, txPowerSta1, txPowerAp2, txPowerSta2,
                 ccaEdThresholdAp1, ccaEdThresholdSta1, ccaEdThresholdAp2, ccaEdThresholdSta2,
                 obssPdThresholdAp1, obssPdThresholdSta1, obssPdThresholdAp2, obssPdThresholdSta2,
                 enableObssPd, packetSize, interval, simulationTime, logFile);

  return 0;
}