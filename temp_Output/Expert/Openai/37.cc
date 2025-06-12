#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

class WifiRateControlComparison
{
public:
  WifiRateControlComparison();
  void Run();

private:
  void ConfigureNodes();
  void ConfigureMobility();
  void ConfigureWifi();
  void ConfigureApplications();
  void ScheduleStaMovement();
  void MoveSta();
  void LogRateAndPower();
  void CheckThroughput();
  void OutputResults();
  std::string WifiManagerName(const std::string &input);

  // Simulation parameters
  std::string m_rateControl;
  std::string m_gnuplotFileThroughput;
  std::string m_gnuplotFilePower;
  uint32_t m_rtsThreshold;
  double m_txPowerStart;
  double m_txPowerEnd;
  double m_txPowerStep;
  double m_simulationTime;
  double m_packetInterval; // seconds
  double m_cbrRateMbps;

  // Node and device handles
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ptr<Node> m_sta;
  Ptr<Node> m_ap;
  Ptr<WifiNetDevice> m_staDevice;
  Ptr<WifiNetDevice> m_apDevice;

  // Mobility & metrics
  Ptr<ConstantPositionMobilityModel> m_apMobility;
  Ptr<ConstantPositionMobilityModel> m_staMobility;
  std::vector<double> m_distances;
  std::vector<double> m_throughputs;
  std::vector<double> m_avgPowers;
  double m_lastRxBytes;
  double m_lastTime;
  double m_txPowerSum;
  uint32_t m_txPowerCount;
  double m_currentDistance;

  // Application
  uint16_t m_port;
  Ptr<PacketSink> m_sink;
};

WifiRateControlComparison::WifiRateControlComparison()
    : m_rateControl("ParfWifiManager"),
      m_gnuplotFileThroughput("throughput.plt"),
      m_gnuplotFilePower("txpower.plt"),
      m_rtsThreshold(2347),
      m_txPowerStart(16.0),
      m_txPowerEnd(16.0),
      m_txPowerStep(0),
      m_simulationTime(30.0),
      m_packetInterval(0.00011759), // for 54Mbps, 1472 bytes: interval = (1472*8)/54e6
      m_cbrRateMbps(54.0),
      m_lastRxBytes(0),
      m_lastTime(0),
      m_txPowerSum(0),
      m_txPowerCount(0),
      m_currentDistance(1.0),
      m_port(5000)
{
}

std::string
WifiRateControlComparison::WifiManagerName(const std::string &input)
{
  if (input == "ParfWifiManager" ||
      input == "AparfWifiManager" ||
      input == "RrpaaWifiManager")
    return input;
  return "ParfWifiManager";
}

void
WifiRateControlComparison::ConfigureNodes()
{
  m_nodes.Create(2); // 0: STA, 1: AP
  m_sta = m_nodes.Get(0);
  m_ap = m_nodes.Get(1);
}

void
WifiRateControlComparison::ConfigureMobility()
{
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // STA
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // AP

  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);

  m_staMobility = m_sta->GetObject<ConstantPositionMobilityModel>();
  m_apMobility = m_ap->GetObject<ConstantPositionMobilityModel>();
}

void
WifiRateControlComparison::ConfigureWifi()
{
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  std::string manager = WifiManagerName(m_rateControl);
  if (manager == "ParfWifiManager")
    wifi.SetRemoteStationManager("ns3::ParfWifiManager", "RtsCtsThreshold", UintegerValue(m_rtsThreshold));
  else if (manager == "AparfWifiManager")
    wifi.SetRemoteStationManager("ns3::AparfWifiManager", "RtsCtsThreshold", UintegerValue(m_rtsThreshold));
  else if (manager == "RrpaaWifiManager")
    wifi.SetRemoteStationManager("ns3::RrpaaWifiManager", "RtsCtsThreshold", UintegerValue(m_rtsThreshold));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(m_txPowerStart));
  phy.Set("TxPowerEnd", DoubleValue(m_txPowerEnd));
  phy.Set("TxPowerLevels", UintegerValue(1));
  phy.Set("TxGain", DoubleValue(0));

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-rate-ctrl");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  m_devices.Add(wifi.Install(phy, mac, m_sta));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  m_devices.Add(wifi.Install(phy, mac, m_ap));

  m_staDevice = DynamicCast<WifiNetDevice>(m_devices.Get(0));
  m_apDevice = DynamicCast<WifiNetDevice>(m_devices.Get(1));
}

void
WifiRateControlComparison::ConfigureApplications()
{
  InternetStackHelper stack;
  stack.Install(m_nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(m_devices);

  m_sink = CreateObject<PacketSink>();
  m_sink->SetAttribute("Protocol", TypeIdValue(UdpSocketFactory::GetTypeId()));
  m_ap->AddApplication(m_sink);
  m_sink->SetStartTime(Seconds(0.0));
  m_sink->SetStopTime(Seconds(m_simulationTime));

  UdpClientHelper client(interfaces.GetAddress(1), m_port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
  client.SetAttribute("Interval", TimeValue(Seconds(m_packetInterval)));
  client.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer apps = client.Install(m_sta);
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(m_simulationTime));
}

void
WifiRateControlComparison::ScheduleStaMovement()
{
  Simulator::Schedule(Seconds(1.0), &WifiRateControlComparison::MoveSta, this);
}

void
WifiRateControlComparison::MoveSta()
{
  m_currentDistance += 1.0;
  Vector pos = m_staMobility->GetPosition();
  pos.x = m_currentDistance;
  m_staMobility->SetPosition(pos);

  LogRateAndPower();
  CheckThroughput();

  if (Simulator::Now().GetSeconds() + 1 < m_simulationTime)
    Simulator::Schedule(Seconds(1.0), &WifiRateControlComparison::MoveSta, this);
}

void
WifiRateControlComparison::LogRateAndPower()
{
  Ptr<WifiPhy> phy = m_staDevice->GetPhy();
  double txpower = phy->GetTxPowerStart();
  m_txPowerSum += txpower;
  ++m_txPowerCount;
  m_avgPowers.push_back(txpower);

  Ptr<WifiRemoteStationManager> mgr = m_staDevice->GetRemoteStationManager();
  std::string rate = mgr->GetDataMode().GetUniqueName();
  uint32_t curDist = static_cast<uint32_t>(m_currentDistance);

  m_distances.push_back(curDist);
}

void
WifiRateControlComparison::CheckThroughput()
{
  Time now = Simulator::Now();
  double rxBytes = m_sink->GetTotalRx();
  double deltaBytes = rxBytes - m_lastRxBytes;
  double deltaTime = now.GetSeconds() - m_lastTime;
  double throughput = (deltaBytes * 8.0) / (deltaTime * 1000000.0); // Mbps
  m_throughputs.push_back(throughput);

  m_lastRxBytes = rxBytes;
  m_lastTime = now.GetSeconds();
}

void
WifiRateControlComparison::OutputResults()
{
  std::ofstream throut(m_gnuplotFileThroughput);
  throut << "#Distance(m)\tAvgThroughput(Mbps)" << std::endl;
  for (size_t i = 0; i < m_distances.size(); ++i) {
    throut << m_distances[i] << "\t" << m_throughputs[i] << std::endl;
  }
  throut.close();

  std::ofstream pout(m_gnuplotFilePower);
  pout << "#Distance(m)\tAvgTxPower(dBm)" << std::endl;
  for (size_t i = 0; i < m_distances.size(); ++i) {
    pout << m_distances[i] << "\t" << m_avgPowers[i] << std::endl;
  }
  pout.close();
}

void
WifiRateControlComparison::Run()
{
  ConfigureNodes();
  ConfigureMobility();
  ConfigureWifi();
  ConfigureApplications();

  Simulator::Schedule(Seconds(1.0), &WifiRateControlComparison::MoveSta, this);

  Simulator::Stop(Seconds(m_simulationTime));
  Simulator::Run();

  OutputResults();
  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  WifiRateControlComparison sim;

  CommandLine cmd;
  std::string rate = "ParfWifiManager";
  uint32_t rts = 2347;
  std::string outfileThr = "throughput.plt";
  std::string outfilePwr = "txpower.plt";
  cmd.AddValue("rateControl", "Rate control manager (ParfWifiManager|AparfWifiManager|RrpaaWifiManager)", rate);
  cmd.AddValue("rtsThreshold", "RTS threshold (bytes)", rts);
  cmd.AddValue("outputThroughput", "Throughput output file", outfileThr);
  cmd.AddValue("outputTxPower", "Tx Power output file", outfilePwr);
  cmd.Parse(argc, argv);

  sim.m_rateControl = rate;
  sim.m_rtsThreshold = rts;
  sim.m_gnuplotFileThroughput = outfileThr;
  sim.m_gnuplotFilePower = outfilePwr;

  sim.Run();
  return 0;
}