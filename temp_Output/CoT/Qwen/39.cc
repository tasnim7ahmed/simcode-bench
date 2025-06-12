#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifiMinstrel");

class ThroughputDistanceLogger
{
public:
  ThroughputDistanceLogger(std::string filename);
  void Log(double distance, double throughput); // Mbps
  void WriteToFile();

private:
  Gnuplot2dDataset m_dataset;
  Gnuplot m_gnuplot;
};

ThroughputDistanceLogger::ThroughputDistanceLogger(std::string filename)
{
  m_gnuplot.SetTitle("Throughput vs Distance");
  m_gnuplot.SetTerminal("png");
  m_gnuplot.AddDataset(m_dataset);
  m_dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
  m_dataset.SetTitle("Throughput vs Distance");
  m_gnuplot.SetLegend("Distance (m)", "Throughput (Mbps)");
  m_gnuplot.SetOutputFileName(filename);
}

void ThroughputDistanceLogger::Log(double distance, double throughput)
{
  m_dataset.Add(distance, throughput);
}

void ThroughputDistanceLogger::WriteToFile()
{
  std::ofstream plotFile;
  plotFile.open(m_gnuplot.GetOutputFileName().c_str());
  m_gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

int main(int argc, char *argv[])
{
  uint32_t numSteps = 10;
  double stepSize = 5.0; // meters
  double stepInterval = 1.0; // seconds
  bool enableLog = false;
  std::string phyMode = "HtMcs7";
  std::string rateControl = "MinstrelWifiManager";

  CommandLine cmd(__FILE__);
  cmd.AddValue("numSteps", "Number of distance steps", numSteps);
  cmd.AddValue("stepSize", "Distance step size in meters", stepSize);
  cmd.AddValue("stepInterval", "Time interval between steps in seconds", stepInterval);
  cmd.AddValue("enableLog", "Enable logging of rate changes", enableLog);
  cmd.Parse(argc, argv);

  if (enableLog)
    {
      LogComponentEnable("RateAdaptiveWifiMinstrel", LOG_LEVEL_INFO);
      LogComponentEnable("WifiRemoteStationManager", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create(2);

  // AP is node 0, STA is node 1
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::" + rateControl);

  WifiMacHelper mac;
  Ssid ssid = Ssid("rate-adaptive-wifi");

  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(2.5)));
  apDevice = wifi.Install(phy, mac, nodes.Get(0));

  NetDeviceContainer staDevice;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  staDevice = wifi.Install(phy, mac, nodes.Get(1));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  // Mobility model: STA moves away from AP in steps
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP fixed at origin
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Initial position of STA
  mobility.SetPositionAllocator(positionAlloc);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // UDP CBR traffic from AP to STA
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(numSteps * stepInterval + 1.0));

  UdpClientHelper client(staInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
  client.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
  client.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(0.0));
  clientApp.Stop(Seconds(numSteps * stepInterval + 1.0));

  // Schedule movement steps
  for (uint32_t i = 0; i < numSteps; ++i)
    {
      double distance = (i + 1) * stepSize;
      Simulator::Schedule(Seconds((i + 1) * stepInterval),
                          &MobilityHelper::SetPosition,
                          &mobility,
                          nodes.Get(1),
                          Vector(distance, 0.0, 0.0));
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  ThroughputDistanceLogger logger("throughput-vs-distance.plt");

  // Schedule throughput logging
  for (uint32_t i = 0; i < numSteps; ++i)
    {
      double logTime = (i + 1) * stepInterval;
      double distance = (i + 1) * stepSize;
      Simulator::Schedule(Seconds(logTime),
                          [distance, &monitor, &logger]()
                          {
                            monitor->CheckForLostPackets();
                            FlowMonitor::FlowStats stats = monitor->GetFlowStats().begin()->second;
                            double duration = 1.0; // Interval duration
                            double throughput = (stats.rxBytes * 8.0 / (duration * 1e6)); // Mbps
                            NS_LOG_INFO("Distance: " << distance << "m, Throughput: " << throughput << " Mbps");
                            logger.Log(distance, throughput);
                          });
    }

  Simulator::Stop(Seconds(numSteps * stepInterval + 1.0));
  Simulator::Run();

  logger.WriteToFile();

  Simulator::Destroy();
  return 0;
}