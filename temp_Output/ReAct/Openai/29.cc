#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMultirateExperiment");

class WifiMultirateExperiment
{
public:
  WifiMultirateExperiment ();
  void Configure (int argc, char **argv);
  void Run ();
  void SetupApplications ();
  void MonitorThroughput ();
  void WriteGnuplot ();
  void ScheduleThroughputMonitor ();
  void MobilityTrace (std::string context, Ptr<const MobilityModel> mobility);

private:
  uint32_t m_nNodes;
  double m_simulationTime;
  double m_areaSize;
  uint32_t m_packetSize;
  double m_appStartTime;
  double m_appStopTime;
  double m_totalRx;
  std::vector<double> m_timeSeries;
  std::vector<double> m_throughputSeries;

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  std::vector<ApplicationContainer> m_serverApps, m_clientApps;

  Gnuplot m_gnuplot;
  Ptr<FlowMonitor> m_flowMonitor;
  Ptr<FlowMonitorHelper> m_flowMonitorHelper;

  bool m_useAodv;

  EventId m_throughputMonitorEvent;
};

WifiMultirateExperiment::WifiMultirateExperiment ()
  : m_nNodes (100),
    m_simulationTime (60.0),
    m_areaSize (500.0),
    m_packetSize (1024),
    m_appStartTime (2.0),
    m_appStopTime (58.0),
    m_totalRx (0.0),
    m_gnuplot ("throughput.png"),
    m_useAodv (true)
{
  m_gnuplot.SetTitle ("Multirate WiFi Throughput");
  m_gnuplot.SetLegend ("Time (s)", "Throughput (Mbps)");
}

void
WifiMultirateExperiment::Configure (int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of ad hoc nodes", m_nNodes);
  cmd.AddValue ("simulationTime", "Duration (s)", m_simulationTime);
  cmd.AddValue ("areaSize", "Simulation area side (m)", m_areaSize);
  cmd.AddValue ("packetSize", "App packet size", m_packetSize);
  cmd.AddValue ("useAodv", "Use AODV (else OLSR)", m_useAodv);
  cmd.Parse (argc, argv);
}

void
WifiMultirateExperiment::Run ()
{
  m_nodes.Create (m_nNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Multirate (auto rate)
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

  wifiMac.SetType ("ns3::AdhocWifiMac");
  m_devices = wifi.Install (wifiPhy, wifiMac, m_nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
      "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
      "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
      "PositionAllocator", PointerValue (
        CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
          "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
          "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]")
        )));
  mobility.Install (m_nodes);

  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeCallback (&WifiMultirateExperiment::MobilityTrace, this));

  // Internet stack + routing
  InternetStackHelper internet;
  if (m_useAodv)
    {
      AodvHelper aodv;
      internet.SetRoutingHelper (aodv);
    }
  else
    {
      OlsrHelper olsr;
      internet.SetRoutingHelper (olsr);
    }
  internet.Install (m_nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.0.0");
  m_interfaces = ipv4.Assign (m_devices);

  // Applications
  SetupApplications ();

  // FlowMonitor
  m_flowMonitorHelper = CreateObject<FlowMonitorHelper> ();
  m_flowMonitor = m_flowMonitorHelper->InstallAll ();

  // Throughput and Gnuplot
  m_totalRx = 0;
  m_timeSeries.clear ();
  m_throughputSeries.clear ();
  m_throughputMonitorEvent = Simulator::Schedule (Seconds (1.0),
      &WifiMultirateExperiment::MonitorThroughput, this);

  Simulator::Stop (Seconds (m_simulationTime));
  Simulator::Run ();

  WriteGnuplot ();

  m_flowMonitor->SerializeToXmlFile ("flowmon.xml", true, true);

  Simulator::Destroy ();
}

void
WifiMultirateExperiment::SetupApplications ()
{
  uint32_t nFlows = std::max (5u, m_nNodes / 10);
  uint16_t port = 5000;

  m_serverApps.resize (nFlows);
  m_clientApps.resize (nFlows);

  for (uint32_t i = 0; i < nFlows; ++i)
    {
      uint32_t serverIdx = i;
      uint32_t clientIdx = (i + 10) % m_nNodes;
      UdpServerHelper server (port + i);
      m_serverApps[i] = server.Install (m_nodes.Get (serverIdx));
      m_serverApps[i].Start (Seconds (m_appStartTime));
      m_serverApps[i].Stop (Seconds (m_appStopTime));

      UdpClientHelper client (m_interfaces.GetAddress (serverIdx), port + i);
      client.SetAttribute ("MaxPackets", UintegerValue (1000000));
      client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
      client.SetAttribute ("PacketSize", UintegerValue (m_packetSize));
      m_clientApps[i] = client.Install (m_nodes.Get (clientIdx));
      m_clientApps[i].Start (Seconds (m_appStartTime + 0.3 * i));
      m_clientApps[i].Stop (Seconds (m_appStopTime));
    }
}

void
WifiMultirateExperiment::MonitorThroughput ()
{
  uint64_t totalRx = 0;
  double curTime = Simulator::Now ().GetSeconds ();

  for (size_t i = 0; i < m_serverApps.size (); ++i)
    {
      Ptr<UdpServer> server = DynamicCast<UdpServer> (m_serverApps[i].Get (0));
      if (server)
        totalRx += server->GetReceived ();
    }

  static uint64_t lastTotalRx = 0;
  double throughput = (totalRx - lastTotalRx) * m_packetSize * 8.0 / 1e6; // Mbps per sec
  m_timeSeries.push_back (curTime);
  m_throughputSeries.push_back (throughput);
  lastTotalRx = totalRx;

  NS_LOG_INFO ("Time " << curTime << "s, Throughput: " << throughput << " Mbps");

  if (curTime < m_simulationTime)
    m_throughputMonitorEvent = Simulator::Schedule (Seconds (1.0),
        &WifiMultirateExperiment::MonitorThroughput, this);
}

void
WifiMultirateExperiment::WriteGnuplot ()
{
  Gnuplot2dDataset dataset ("Total Throughput");
  for (size_t i = 0; i < m_timeSeries.size (); ++i)
    dataset.Add (m_timeSeries[i], m_throughputSeries[i]);

  m_gnuplot.AddDataset (dataset);
  std::ofstream out ("throughput.plt");
  m_gnuplot.GenerateOutput (out);
  out.close ();
  NS_LOG_UNCOND ("Gnuplot file throughput.plt and throughput.png created.");
}

void
WifiMultirateExperiment::MobilityTrace (std::string context, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  NS_LOG_DEBUG ("Mobility event: " << context << " pos=" << pos);
}

int
main (int argc, char **argv)
{
  LogComponentEnable ("WifiMultirateExperiment", LOG_LEVEL_INFO);
  // Uncomment next for debugging mobility or simulation issues
  // LogComponentEnable ("MobilityModel", LOG_LEVEL_DEBUG);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);

  WifiMultirateExperiment exp;
  exp.Configure (argc, argv);
  exp.Run ();

  return 0;
}

/*
Build Instructions:
  (in ns-3 top dir)
  $ ./waf configure --enable-examples
  $ ./waf build
  $ ./waf --run "scratch/<filename> [options]"

Options:
  --nNodes=X                Number of wireless nodes (default 100)
  --simulationTime=Y        Simulation duration in seconds
  --areaSize=Z              Size of square field (meters)
  --packetSize=W            App packet size (bytes)
  --useAodv=[1|0]           Use AODV (1) or OLSR (0)

Generated files:
  throughput.plt, throughput.png, flowmon.xml

To view the Gnuplot data:
  $ gnuplot throughput.plt
Debugging:
  Enable more logging by uncommenting LogComponentEnable lines in main().
*/