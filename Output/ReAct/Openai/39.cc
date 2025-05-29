#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMinstrelRateAdaptationExample");

// UDP Throughput statistics helper
class ThroughputStats : public Object
{
public:
  ThroughputStats(ApplicationContainer apps, Ptr<Node> sta, Ptr<ConstantPositionMobilityModel> apMob, Ptr<MobilityModel> staMob,
                  Time interval, double distanceStep, uint32_t steps, bool logging, std::string gnuplotFile)
    : m_apps(apps), m_sta(sta), m_apMob(apMob), m_staMob(staMob),
      m_interval(interval), m_distanceStep(distanceStep), m_totalSteps(steps),
      m_logging(logging), m_gnuplotFile(gnuplotFile), m_currentStep(0)
  {
    m_sink = DynamicCast<PacketSink>(apps.Get(1));
  }

  void Start()
  {
    m_results.clear();
    ScheduleNextStep();
  }

private:
  void ScheduleNextStep()
  {
    Simulator::Schedule(MilliSeconds(1), &ThroughputStats::DoStep, this);
  }

  void DoStep()
  {
    double distance = m_staMob->GetDistanceFrom(m_apMob);
    uint64_t totalRx = m_sink->GetTotalRx();
    Time now = Simulator::Now();
    double throughput = (totalRx - m_lastTotalRx) * 8.0 / 1e6 / m_interval.GetSeconds(); // Mbps

    if (m_logging)
    {
      std::cout << "Time: " << now.GetSeconds()
                << "s, Distance: " << std::setw(6) << std::fixed << std::setprecision(2) << distance
                << " m, Throughput: " << std::setw(7) << std::setprecision(2) << throughput << " Mbps\n";
    }

    m_results.push_back(std::make_pair(distance, throughput));

    m_lastTotalRx = totalRx;

    if (++m_currentStep < m_totalSteps)
    {
      // Move STA farther
      double newX = m_staMob->GetPosition().x + m_distanceStep;
      Vector pos = m_staMob->GetPosition();
      pos.x = newX;
      m_staMob->SetPosition(pos);
      Simulator::Schedule(m_interval, &ThroughputStats::DoStep, this);
    }
    else
    {
      OutputGnuplot();
    }
  }

  void OutputGnuplot()
  {
    Gnuplot plot("throughput-vs-distance.png");
    plot.SetTitle("UDP Throughput vs Distance (Minstrel Rate Adaptation)");
    plot.SetLegend("Distance (m)", "Throughput (Mbps)");
    Gnuplot2dDataset dataset;
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    std::ofstream outf(m_gnuplotFile);
    outf << "#Distance(m)\tThroughput(Mbps)\n";
    for (const auto& pair : m_results)
    {
      dataset.Add(pair.first, pair.second);
      outf << pair.first << "\t" << pair.second << "\n";
    }
    outf.close();
    plot.AddDataset(dataset);
    std::ofstream plotFile("throughput-vs-distance.plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();
    if (m_logging)
    {
      std::cout << "Throughput-vs-distance data written to " << m_gnuplotFile << "\n";
    }
  }

  ApplicationContainer m_apps;
  Ptr<PacketSink> m_sink;
  Ptr<Node> m_sta;
  Ptr<ConstantPositionMobilityModel> m_apMob;
  Ptr<MobilityModel> m_staMob;

  Time m_interval;
  double m_distanceStep;
  uint32_t m_totalSteps;
  bool m_logging;
  std::string m_gnuplotFile;

  uint32_t m_currentStep;
  uint64_t m_lastTotalRx = 0;
  std::vector<std::pair<double, double>> m_results;
};

// Logging function for rate changes (if supported in NS-3 version)
static void
RateChangeCallback(Ptr<const WifiMac> mac, WifiMode oldMode, WifiMode newMode)
{
  std::cout << "Rate change at " << Simulator::Now().GetSeconds()
            << "s: " << oldMode.GetDataRate(Mbps) << " Mbps -> "
            << newMode.GetDataRate(Mbps) << " Mbps\n";
}

int main(int argc, char *argv[])
{
  double startDistance = 1.0;
  double distanceStep = 2.0;
  uint32_t steps = 20;
  double stepInterval = 1.0;
  bool logging = false;
  std::string gnuplotFile = "throughput-vs-distance.dat";

  CommandLine cmd(__FILE__);
  cmd.AddValue("startDistance", "Initial AP-STA separation (meters)", startDistance);
  cmd.AddValue("distanceStep", "Distance increase per step (meters)", distanceStep);
  cmd.AddValue("steps", "Number of distance steps", steps);
  cmd.AddValue("stepInterval", "Time per step (seconds)", stepInterval);
  cmd.AddValue("logging", "Enable logging (true/false)", logging);
  cmd.AddValue("gnuplotFile", "File to write throughput vs. distance data", gnuplotFile);
  cmd.Parse(argc, argv);

  if (logging)
    LogComponentEnable("WifiMinstrelRateAdaptationExample", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer wifiNodes;
  wifiNodes.Create(2);
  Ptr<Node> apNode = wifiNodes.Get(0);
  Ptr<Node> staNode = wifiNodes.Get(1);

  // Wifi channel and PHY/helper settings
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
  // AP: any power/rate control (let default), STA: force Minstrel
  Ssid ssid = Ssid("wifi-minstrel");

  // AP
  NetDeviceContainer apDevice;
  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  apDevice = wifi.Install(wifiPhy, wifiMac, apNode);

  // STA
  NetDeviceContainer staDevice;
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
  staDevice = wifi.Install(wifiPhy, wifiMac, staNode);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(wifiNodes);
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevice, staDevice));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));           // AP at origin
  positionAlloc->Add(Vector(startDistance, 0.0, 0.0)); // STA at X
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  Ptr<ConstantPositionMobilityModel> apMob = apNode->GetObject<ConstantPositionMobilityModel>();
  Ptr<MobilityModel> staMob = staNode->GetObject<MobilityModel>();

  // Applications: AP sends UDP to STA (CBR) at 400Mbps
  uint16_t port = 4000;
  double appStart = 0.05;
  double simTime = stepInterval * steps + 1.0;

  // UDP Server (sink) on STA
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  ApplicationContainer apps = sink.Install(staNode);

  // UDP Client on AP
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetConstantRate(DataRate("400Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime - 0.01)));
  ApplicationContainer senderApp = onoff.Install(apNode);

  apps.Add(senderApp);

  // Rate change logging, if enabled
  if (logging)
  {
    Ptr<NetDevice> staNetDevice = staDevice.Get(0);
    Ptr<WifiNetDevice> wifiStaNetDevice = DynamicCast<WifiNetDevice>(staNetDevice);
    Ptr<WifiMac> staMac = wifiStaNetDevice->GetMac();
    staMac->TraceConnectWithoutContext("TxMode", MakeCallback(&RateChangeCallback));
  }

  // Throughput/distance stats
  Ptr<ThroughputStats> tputStats = CreateObject<ThroughputStats>(apps, staNode, apMob, staMob,
                                                                 Seconds(stepInterval), distanceStep, steps, logging, gnuplotFile);
  Simulator::Schedule(Seconds(appStart), &ThroughputStats::Start, tputStats);

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}