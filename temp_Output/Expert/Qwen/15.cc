#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughput");

class WifiMimoExperiment {
public:
  WifiMimoExperiment();
  void Run(uint32_t mcs, uint32_t nStreams, double distance);
  void SetTcp(bool tcp) { m_tcp = tcp; }
  void SetFrequency(double freq) { m_frequency = freq; }
  void SetChannelWidth(uint16_t width) { m_channelWidth = width; }
  void SetShortGuardInterval(bool sgi) { m_shortGuardInterval = sgi; }
  void SetPreambleDetection(bool pd) { m_preambleDetection = pd; }
  void SetSimulationTime(double t) { m_simTime = t; }
  void SetStepSize(double s) { m_stepSize = s; }

private:
  void Setup();
  void Teardown();
  void CalculateThroughput();

  bool m_tcp;
  double m_frequency;
  uint16_t m_channelWidth;
  bool m_shortGuardInterval;
  bool m_preambleDetection;
  double m_simTime;
  double m_stepSize;
  Gnuplot2dDataset m_datasets[4][4]; // HT MCS 0-3, streams 1-4

  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apIpIfaces;
  Ipv4InterfaceContainer m_staIpIfaces;
  Ptr<FlowMonitor> m_flowMonitor;
};

WifiMimoExperiment::WifiMimoExperiment()
  : m_tcp(false),
    m_frequency(5.0),
    m_channelWidth(20),
    m_shortGuardInterval(false),
    m_preambleDetection(true),
    m_simTime(10.0),
    m_stepSize(10.0)
{
  for (uint32_t i = 0; i < 4; ++i) {
    for (uint32_t j = 0; j < 4; ++j) {
      std::ostringstream oss;
      oss << "HT-MCS" << j << "-" << (i+1) << "Stream";
      m_datasets[i][j].SetTitle(oss.str());
      m_datasets[i][j].SetStyle(Gnuplot2dDataset::LINES_POINTS);
    }
  }
}

void WifiMimoExperiment::Setup() {
  m_apNode.Create(1);
  m_staNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs3"),
                               "ControlMode", StringValue("HtMcs3"));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("mimo-network")),
              "ActiveProbing", BooleanValue(false));

  m_staDev = wifi.Install(phy, mac, m_staNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("mimo-network")));

  m_apDev = wifi.Install(phy, mac, m_apNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(0.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNode);

  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apIpIfaces = address.Assign(m_apDev);
  m_staIpIfaces = address.Assign(m_staDev);

  if (m_tcp) {
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApp = sink.Install(m_apNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(m_simTime + 0.1));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(m_apIpIfaces.GetAddress(0), 9));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));

    ApplicationContainer app = onoff.Install(m_staNode.Get(0));
    app.Start(Seconds(0.1));
    app.Stop(Seconds(m_simTime));
  } else {
    UdpServerHelper server;
    ApplicationContainer sinkApp = server.Install(m_apNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(m_simTime + 0.1));

    UdpClientHelper client(m_apIpIfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    client.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer app = client.Install(m_staNode.Get(0));
    app.Start(Seconds(0.1));
    app.Stop(Seconds(m_simTime));
  }

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(1));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Frequency", DoubleValue(m_frequency * 1e9));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(m_channelWidth));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue(m_shortGuardInterval));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/UseGreenfieldProtection", BooleanValue(!m_preambleDetection));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/MpduAggregator/MaxAmpduSize", UintegerValue(65535));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/BlockAckThreshold", UintegerValue(2));
}

void WifiMimoExperiment::Teardown() {
  m_apNode.Clear();
  m_staNode.Clear();
  m_apDev.Clear();
  m_staDev.Clear();
  m_apIpIfaces = Ipv4InterfaceContainer();
  m_staIpIfaces = Ipv4InterfaceContainer();
}

void WifiMimoExperiment::CalculateThroughput() {
  FlowMonitorHelper flowmon;
  m_flowMonitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(m_simTime));
  Simulator::Run();

  m_flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitor->GetFlowStats();

  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
    if (t.destinationPort == 9) {
      double throughput = (iter->second.rxBytes * 8.0) / (1e6 * iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds());
      NS_LOG_UNCOND("Distance: " << m_staNode.Get(0)->GetObject<MobilityModel>()->GetPosition().x << " Throughput: " << throughput << " Mbps");
    }
  }

  Simulator::Destroy();
}

void WifiMimoExperiment::Run(uint32_t mcs, uint32_t nStreams, double distance) {
  Setup();

  Vector position = m_staNode.Get(0)->GetObject<MobilityModel>()->GetPosition();
  position.x = distance;
  m_staNode.Get(0)->GetObject<MobilityModel>()->SetPosition(position);

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue("HtMcs" + std::to_string(mcs)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ControlMode", StringValue("HtMcs" + std::to_string(mcs)));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/NumberOfTransmitAntennas", UintegerValue(nStreams));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/NumberOfReceiveAntennas", UintegerValue(nStreams));

  CalculateThroughput();
  double thr = 0.0;
  FlowMonitor::FlowStats stats;
  if (m_flowMonitor->GetFlowStats().size() > 0) {
    stats = m_flowMonitor->GetFlowStats().begin()->second;
    thr = (stats.rxBytes * 8.0) / (1e6 * (stats.timeLastRxPacket.GetSeconds() - stats.timeFirstTxPacket.GetSeconds()));
  }

  m_datasets[nStreams - 1][mcs].Add(distance, thr);
  Teardown();
}

int main(int argc, char *argv[]) {
  uint32_t mcs = 0;
  uint32_t nStreams = 1;
  double maxDistance = 100.0;
  double simTime = 10.0;
  double stepSize = 10.0;
  bool tcp = false;
  double frequency = 5.0;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = false;
  bool preambleDetection = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("mcs", "HT MCS value (0-3)", mcs);
  cmd.AddValue("streams", "Number of MIMO streams (1-4)", nStreams);
  cmd.AddValue("distance", "Maximum distance to simulate", maxDistance);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("stepSize", "Distance step size", stepSize);
  cmd.AddValue("tcp", "Use TCP instead of UDP", tcp);
  cmd.AddValue("frequency", "WiFi frequency band (2.4 or 5.0 GHz)", frequency);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGuardInterval", "Enable Short Guard Interval", shortGuardInterval);
  cmd.AddValue("preambleDetection", "Enable Preamble Detection", preambleDetection);
  cmd.Parse(argc, argv);

  NS_ABORT_MSG_IF(mcs > 3, "Invalid MCS index: must be between 0 and 3");
  NS_ABORT_MSG_IF(nStreams < 1 || nStreams > 4, "Invalid number of streams: must be between 1 and 4");

  WifiMimoExperiment experiment;
  experiment.SetTcp(tcp);
  experiment.SetFrequency(frequency);
  experiment.SetChannelWidth(channelWidth);
  experiment.SetShortGuardInterval(shortGuardInterval);
  experiment.SetPreambleDetection(preambleDetection);
  experiment.SetSimulationTime(simTime);
  experiment.SetStepSize(stepSize);

  for (double d = 10.0; d <= maxDistance; d += stepSize) {
    for (uint32_t m = 0; m <= 3; ++m) {
      for (uint32_t s = 1; s <= 4; ++s) {
        experiment.Run(m, s, d);
      }
    }
  }

  Gnuplot plot("wifi-mimo-throughput.png");
  plot.SetTitle("Throughput vs Distance for 802.11n MIMO");
  plot.SetTerminal("png");
  plot.SetLegend("Distance (m)", "Throughput (Mbps)");
  plot.AppendExtra("set key top left box opaque");

  for (uint32_t i = 0; i < 4; ++i) {
    for (uint32_t j = 0; j < 4; ++j) {
      plot.AddDataset(experiment.m_datasets[i][j]);
    }
  }

  std::ofstream plotFile("wifi-mimo-throughput.plt");
  plot.GenerateOutput(plotFile);
  plotFile.close();

  return 0;
}