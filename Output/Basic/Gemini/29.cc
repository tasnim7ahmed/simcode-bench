#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMultirateExperiment");

class WifiMultirateExperiment
{
public:
  WifiMultirateExperiment ();
  ~WifiMultirateExperiment ();
  void Run ();

private:
  void CreateNodes ();
  void InstallWifiDevices ();
  void InstallInternetStack ();
  void InstallApplications ();
  void SetupMobility ();
  void EnablePcapTracing ();
  void EnableFlowMonitor ();
  void CalculateThroughput ();
  void GenerateGnuplotScript ();

  uint32_t m_numNodes;
  double m_simulationTime;
  std::string m_rate;
  std::string m_phyMode;
  bool m_pcapTracing;
  bool m_enableMonitor;
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  FlowMonitorHelper m_flowmonHelper;
  FlowMonitor* m_flowmon;

  // Gnuplot
  Gnuplot m_gnuplot;
  std::vector<double> m_throughputValues;
};

WifiMultirateExperiment::WifiMultirateExperiment ()
  : m_numNodes (100),
    m_simulationTime (10.0),
    m_rate ("OfdmRate6Mbps"),
    m_phyMode ("DsssRate11Mbps"),
    m_pcapTracing (false),
    m_enableMonitor (true)
{
  LogComponent::SetLogLevel( "UdpClient", LOG_LEVEL_INFO );
  LogComponent::SetLogLevel( "UdpServer", LOG_LEVEL_INFO );
}

WifiMultirateExperiment::~WifiMultirateExperiment ()
{
}

void
WifiMultirateExperiment::Run ()
{
  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes", m_numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", m_simulationTime);
  cmd.AddValue ("rate", "Wifi data rate", m_rate);
  cmd.AddValue ("phyMode", "Wifi PHY mode", m_phyMode);
  cmd.AddValue ("pcap", "Enable PCAP tracing", m_pcapTracing);
  cmd.AddValue ("monitor", "Enable Flow Monitor", m_enableMonitor);

  cmd.ParseWithoutEnvironmentVariables ();

  NS_LOG_INFO ("Number of nodes: " << m_numNodes);
  NS_LOG_INFO ("Simulation time: " << m_simulationTime);
  NS_LOG_INFO ("Wifi data rate: " << m_rate);
  NS_LOG_INFO ("Wifi PHY mode: " << m_phyMode);
  NS_LOG_INFO ("PCAP tracing enabled: " << m_pcapTracing);
  NS_LOG_INFO ("Flow Monitor enabled: " << m_enableMonitor);

  CreateNodes ();
  InstallWifiDevices ();
  InstallInternetStack ();
  InstallApplications ();
  SetupMobility ();

  if (m_pcapTracing)
    {
      EnablePcapTracing ();
    }

  if (m_enableMonitor)
    {
      EnableFlowMonitor ();
    }

  Simulator::Stop (Seconds (m_simulationTime));
  Simulator::Run ();

  if (m_enableMonitor)
    {
      CalculateThroughput ();
    }

  Simulator::Destroy ();

  if (m_enableMonitor)
    {
      GenerateGnuplotScript ();
    }
}

void
WifiMultirateExperiment::CreateNodes ()
{
  NS_LOG_FUNCTION (this);
  m_nodes.Create (m_numNodes);
}

void
WifiMultirateExperiment::InstallWifiDevices ()
{
  NS_LOG_FUNCTION (this);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (m_rate),
                                "ControlMode", StringValue (m_phyMode));

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  m_devices = wifi.Install (wifiPhy, wifiMac, m_nodes);
}

void
WifiMultirateExperiment::InstallInternetStack ()
{
  NS_LOG_FUNCTION (this);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (m_nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  m_interfaces = ipv4.Assign (m_devices);
}

void
WifiMultirateExperiment::InstallApplications ()
{
  NS_LOG_FUNCTION (this);

  uint16_t port = 9;

  for (uint32_t i = 0; i < m_numNodes / 2; ++i)
    {
      UdpServerHelper server (port);
      ApplicationContainer apps = server.Install (m_nodes.Get (i + m_numNodes / 2));
      apps.Start (Seconds (1.0));
      apps.Stop (Seconds (m_simulationTime - 1.0));

      UdpClientHelper client (m_interfaces.GetAddress (i + m_numNodes / 2), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      client.SetAttribute ("Interval", TimeValue (Time ("0.01")));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      apps = client.Install (m_nodes.Get (i));
      apps.Start (Seconds (2.0));
      apps.Stop (Seconds (m_simulationTime - 2.0));
    }
}

void
WifiMultirateExperiment::SetupMobility ()
{
  NS_LOG_FUNCTION (this);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=50]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
  mobility.Install (m_nodes);
}

void
WifiMultirateExperiment::EnablePcapTracing ()
{
  NS_LOG_FUNCTION (this);
  WifiHelper wifi;
  wifi.EnablePcapAll ("wifi-multirate");
}

void
WifiMultirateExperiment::EnableFlowMonitor ()
{
  NS_LOG_FUNCTION (this);
  m_flowmonHelper.SetMonitorAttribute ("MaxPacketsPerFlow", UintegerValue (1000000));
  m_flowmon = m_flowmonHelper.InstallAll ();
}

void
WifiMultirateExperiment::CalculateThroughput ()
{
  NS_LOG_FUNCTION (this);

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (m_flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowmon->GetFlowStats ();

  double totalThroughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
      NS_LOG_INFO ("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") : Throughput: " << throughput << " Mbps");
      m_throughputValues.push_back(throughput);
      totalThroughput += throughput;
    }

  NS_LOG_INFO ("Total Throughput: " << totalThroughput << " Mbps");
}

void
WifiMultirateExperiment::GenerateGnuplotScript ()
{
  NS_LOG_FUNCTION (this);

  std::string fileNameWithNoExtension = "wifi-multirate-throughput";
  std::string graphicsFileName        = fileNameWithNoExtension + ".png";
  std::string plotFileName            = fileNameWithNoExtension + ".plt";
  std::string dataFileName            = fileNameWithNoExtension + ".dat";

  // Set the output file name
  m_gnuplot.SetFilename (plotFileName);

  m_gnuplot.SetTitle ("Wifi Multirate Experiment Throughput");
  m_gnuplot.SetLegend ("Flow ID", "Throughput (Mbps)");

  // Set the ranges for X and Y axes
  m_gnuplot.SetXrange (0, m_throughputValues.size() + 1);
  m_gnuplot.SetYrange (0, *std::max_element(m_throughputValues.begin(), m_throughputValues.end()) * 1.1);

  // Configure the plot output
  m_gnuplot.SetTerminal ("png");
  m_gnuplot.AppendOutputFormat ("png");
  m_gnuplot.AppendToFile (dataFileName);

  // Add the plot data
  for (size_t i = 0; i < m_throughputValues.size(); ++i) {
    m_gnuplot.AddData (i+1, m_throughputValues[i]);
  }

  // Generate the plot file
  m_gnuplot.GenerateOutput (graphicsFileName);
}

int
main (int argc, char *argv[])
{
  WifiMultirateExperiment experiment;
  experiment.Run ();

  return 0;
}