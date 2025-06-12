#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifi");

class RateAdaptiveSimulation {
public:
  RateAdaptiveSimulation();
  void Run(int argc, char *argv[]);
private:
  uint32_t m_staCount;
  double m_initialDistance;
  double m_distanceStep;
  double m_stepTime;
  double m_totalTime;
  bool m_loggingEnabled;
  NodeContainer m_apNode;
  NodeContainer m_staNodes;
  NetDeviceContainer m_apDevice;
  NetDeviceContainer m_staDevices;
  Ipv4InterfaceContainer m_apInterface;
  Ipv4InterfaceContainer m_staInterfaces;
  Gnuplot2dDataset m_throughputDataset;
  std::vector<double> m_distances;
  std::vector<double> m_throughputs;
  Ptr<OutputStreamWrapper> m_cbrStream;

  void SetupSimulation();
  void SetupMobility();
  void SetupApplications();
  void ScheduleDistanceChange();
  void RecordMetrics(double distance);
  void OutputGnuplot();
};

RateAdaptiveSimulation::RateAdaptiveSimulation()
  : m_staCount(1),
    m_initialDistance(10.0),
    m_distanceStep(5.0),
    m_stepTime(1.0),
    m_totalTime(10.0),
    m_loggingEnabled(false)
{
}

void RateAdaptiveSimulation::Run(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("initialDistance", "Initial distance between AP and STA (meters)", m_initialDistance);
  cmd.AddValue("distanceStep", "Distance increment per step (meters)", m_distanceStep);
  cmd.AddValue("stepTime", "Time interval between steps (seconds)", m_stepTime);
  cmd.AddValue("totalTime", "Total simulation time (seconds)", m_totalTime);
  cmd.AddValue("logging", "Enable logging output", m_loggingEnabled);
  cmd.Parse(argc, argv);

  if (m_loggingEnabled) {
    LogComponentEnable("RateAdaptiveWifi", LOG_LEVEL_INFO);
  }

  SetupSimulation();

  Simulator::Stop(Seconds(m_totalTime));
  Simulator::Run();
  OutputGnuplot();
  Simulator::Destroy();
}

void RateAdaptiveSimulation::SetupSimulation()
{
  // Create nodes
  m_apNode.Create(1);
  m_staNodes.Create(m_staCount);

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiMacHelper mac;
  Ssid ssid = Ssid("rate-adaptive-wifi");

  // Configure AP
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  m_apDevice = wifi.Install(phy, mac, m_apNode);

  // Configure STA with Minstrel rate control
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
  m_staDevices = wifi.Install(phy, mac, m_staNodes);

  // Setup mobility
  SetupMobility();

  // Install internet stack
  InternetStackHelper stack;
  stack.Install(m_apNode);
  stack.Install(m_staNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  m_apInterface = address.Assign(m_apDevice);
  m_staInterfaces = address.Assign(m_staDevices);

  // Setup applications
  SetupApplications();
}

void RateAdaptiveSimulation::SetupMobility()
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

  // Initial position for STA
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(m_initialDistance, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(m_staNodes);

  // Start moving after initial setup
  ScheduleDistanceChange();
}

void RateAdaptiveSimulation::ScheduleDistanceChange()
{
  double currentDistance = m_initialDistance;
  uint32_t steps = static_cast<uint32_t>(m_totalTime / m_stepTime);
  for (uint32_t i = 0; i < steps; ++i) {
    currentDistance += m_distanceStep;
    Simulator::Schedule(Seconds((i + 1) * m_stepTime), &RateAdaptiveSimulation::RecordMetrics, this, currentDistance);
  }
}

void RateAdaptiveSimulation::RecordMetrics(double distance)
{
  if (m_loggingEnabled) {
    NS_LOG_INFO("Setting STA distance to " << distance << " meters");
  }

  // Move the STA
  Ptr<Node> staNode = m_staNodes.Get(0);
  Ptr<MobilityModel> mob = staNode->GetObject<MobilityModel>();
  Vector pos = mob->GetPosition();
  mob->SetPosition(Vector(distance, pos.y, pos.z));

  // Capture throughput
  double throughput = 0.0;
  if (!m_cbrStream.IsZero()) {
    *m_cbrStream->GetStream() << "Capturing throughput at distance: " << distance << std::endl;
    // Throughput would be calculated from received data in application layer
  }

  m_distances.push_back(distance);
  m_throughputs.push_back(throughput);
}

void RateAdaptiveSimulation::SetupApplications()
{
  // UDP CBR traffic from AP to STA
  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(m_staInterfaces.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(m_staNodes.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(m_totalTime));

  OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("400Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer sourceApps = onoff.Install(m_apNode.Get(0));
  sourceApps.Start(Seconds(0.5));
  sourceApps.Stop(Seconds(m_totalTime - 0.1));

  m_cbrStream = Create<OutputStreamWrapper>("cbr-application.log", std::ios::out);
  packetSinkHelper.TraceConnectWithoutContext("Rx", MakeBoundCallback(&PacketSink::Write, m_cbrStream));
}

void RateAdaptiveSimulation::OutputGnuplot()
{
  Gnuplot plot("throughput-vs-distance.plt");
  plot.SetTitle("Throughput vs Distance");
  plot.SetTerminal("png");
  plot.SetLegend("Distance (m)", "Throughput (Mbps)");
  plot.AppendExtra("set grid");

  m_throughputDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  m_throughputDataset.SetTitle("Throughput");

  for (size_t i = 0; i < m_distances.size(); ++i) {
    m_throughputDataset.Add(m_distances[i], m_throughputs[i]);
  }

  plot.AddDataset(m_throughputDataset);

  std::ofstream plotFile(plot.GetName().c_str());
  plot.GenerateOutput(plotFile);
  plotFile.close();
}

int main(int argc, char *argv[])
{
  RateAdaptiveSimulation sim;
  sim.Run(argc, argv);
  return 0;
}