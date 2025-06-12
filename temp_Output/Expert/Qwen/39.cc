#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifiSimulation");

class RateAdaptiveWifiExperiment
{
public:
  RateAdaptiveWifiExperiment();
  void Setup(int argc, char *argv[]);
  void Run();
  void Report();

private:
  uint32_t m_staCount;
  double m_simTime;
  double m_distanceStep;
  double m_stepInterval;
  bool m_enableLog;
  std::string m_outputFileName;

  NodeContainer m_apNode;
  NodeContainer m_staNodes;
  NetDeviceContainer m_apDevices;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterfaces;
  Ipv4InterfaceContainer m_staInterfaces;

  Gnuplot2dDataset m_throughputDataset;
  std::vector<double> m_distances;
  std::vector<double> m_throughputs;

  void CreateNodes();
  void CreateDevices();
  void InstallApplications();
  void MoveSta(double distance);
  void ScheduleMovements();
  void MeasureThroughput();
};

RateAdaptiveWifiExperiment::RateAdaptiveWifiExperiment()
  : m_staCount(1),
    m_simTime(10.0),
    m_distanceStep(10.0),
    m_stepInterval(2.0),
    m_enableLog(false),
    m_outputFileName("throughput-vs-distance")
{
}

void RateAdaptiveWifiExperiment::Setup(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("distanceStep", "Distance step (meters)", m_distanceStep);
  cmd.AddValue("stepInterval", "Time between steps (seconds)", m_stepInterval);
  cmd.AddValue("enableLog", "Enable logging", m_enableLog);
  cmd.AddValue("outputFile", "Output file name for gnuplot", m_outputFileName);
  cmd.Parse(argc, argv);

  if (m_enableLog)
    {
      LogComponentEnable("RateAdaptiveWifiSimulation", LOG_LEVEL_INFO);
      LogComponentEnable("MinstrelWifiManager", LOG_LEVEL_ALL);
    }

  m_simTime = m_stepInterval * static_cast<double>(static_cast<int>(100 / m_distanceStep));
  m_apNode.Create(1);
  m_staNodes.Create(m_staCount);
}

void RateAdaptiveWifiExperiment::CreateNodes()
{
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

  // Initial position of STA at origin
  mobility.Install(m_staNodes);
}

void RateAdaptiveWifiExperiment::CreateDevices()
{
  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager"); // Only Minstrel on STA

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  // AP uses default rate control
  wifiMac.SetType("ns3::ApWifiMac");
  m_apDevices = wifi.Install(wifiPhy, wifiMac, m_apNode);

  // STA uses Minstrel
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
  m_staDevices = wifi.Install(wifiPhy, wifiMac, m_staNodes);

  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_apInterfaces = address.Assign(m_apDevices);
  m_staInterfaces = address.Assign(m_staDevices);
}

void RateAdaptiveWifiExperiment::InstallApplications()
{
  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(m_staInterfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(m_staNodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(m_simTime));

  OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("400Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer srcApps = onoff.Install(m_apNode.Get(0));
  srcApps.Start(Seconds(0.0));
  srcApps.Stop(Seconds(m_simTime));
}

void RateAdaptiveWifiExperiment::MoveSta(double distance)
{
  NS_LOG_INFO("Moving STA to distance: " << distance << " meters");
  Ptr<Node> staNode = m_staNodes.Get(0);
  Ptr<MobilityModel> mob = staNode->GetObject<MobilityModel>();
  Vector pos = mob->GetPosition();
  pos.x = distance;
  mob->SetPosition(pos);
}

void RateAdaptiveWifiExperiment::ScheduleMovements()
{
  double time = 0.0;
  double distance = 0.0;
  while (time < m_simTime)
    {
      Simulator::Schedule(Seconds(time), &RateAdaptiveWifiExperiment::MoveSta, this, distance);
      m_distances.push_back(distance);
      time += m_stepInterval;
      distance += m_distanceStep;
    }
}

void RateAdaptiveWifiExperiment::MeasureThroughput()
{
  double totalRxBytes = 0;
  for (uint32_t i = 0; i < m_staNodes.GetN(); ++i)
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink>(m_staNodes.Get(i)->GetApplication(0));
      totalRxBytes += sink->GetTotalRx();
    }
  double throughput = (totalRxBytes * 8) / (m_simTime * 1e6); // Mbps
  NS_LOG_INFO("Measured throughput: " << throughput << " Mbps");
  m_throughputs.push_back(throughput);
}

void RateAdaptiveWifiExperiment::Run()
{
  CreateNodes();
  CreateDevices();
  InstallApplications();
  ScheduleMovements();
  Simulator::Schedule(Seconds(m_simTime), &RateAdaptiveWifiExperiment::MeasureThroughput, this);
  Simulator::Stop(Seconds(m_simTime));
  Simulator::Run();
  Simulator::Destroy();
}

void RateAdaptiveWifiExperiment::Report()
{
  Gnuplot gnuplot((m_outputFileName + ".plt").c_str());
  gnuplot.SetTitle("Throughput vs Distance");
  gnuplot.SetTerminal("png");
  gnuplot.SetOutputFilename((m_outputFileName + ".png").c_str());
  gnuplot.SetLegend("Distance (m)", "Throughput (Mbps)");

  m_throughputDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  for (size_t i = 0; i < m_distances.size(); ++i)
    {
      m_throughputDataset.Add(m_distances[i], m_throughputs[i]);
    }

  gnuplot.AddDataset(m_throughputDataset);

  std::ofstream plotFile((m_outputFileName + ".plt").c_str());
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  std::ofstream dataFile((m_outputFileName + ".dat").c_str());
  for (size_t i = 0; i < m_distances.size(); ++i)
    {
      dataFile << m_distances[i] << " " << m_throughputs[i] << std::endl;
    }
  dataFile.close();
}

int main(int argc, char *argv[])
{
  RateAdaptiveWifiExperiment experiment;
  experiment.Setup(argc, argv);
  experiment.Run();
  experiment.Report();
}