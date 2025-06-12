#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/data-collection-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistanceExperiment");

class WifiPerformanceExperiment
{
public:
  WifiPerformanceExperiment();
  void Run(uint32_t distance, std::string format, uint32_t runId);
  void TransmitCallback(Ptr<Packet const> packet);
  void ReceiveCallback(Ptr<Packet const> packet);

private:
  void SetupNodes();
  void SetupMobility();
  void SetupWifi();
  void SetupApplications();
  void SetupStats();

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  ApplicationContainer m_apps;
  DataCollector m_dataCollector;
  uint64_t m_txCount;
  uint64_t m_rxCount;
};

WifiPerformanceExperiment::WifiPerformanceExperiment()
  : m_txCount(0),
    m_rxCount(0)
{
}

void
WifiPerformanceExperiment::TransmitCallback(Ptr<Packet const> packet)
{
  m_txCount++;
  m_dataCollector.DescribeRun(m_runId, "wifi-experiment", "distance", std::to_string(distance));
  m_dataCollector.AddData("Packets", "Tx", 1);
}

void
WifiPerformanceExperiment::ReceiveCallback(Ptr<Packet const> packet)
{
  m_rxCount++;
  m_dataCollector.AddData("Packets", "Rx", 1);
}

void
WifiPerformanceExperiment::SetupNodes()
{
  m_nodes.Create(2);

  InternetStackHelper stack;
  stack.Install(m_nodes);

  PointToPointHelper p2p;
  p2p.Install(m_nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  m_interfaces = address.Assign(m_devices);
}

void
WifiPerformanceExperiment::SetupMobility()
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);
}

void
WifiPerformanceExperiment::SetupWifi()
{
  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211b);

  wifiMac.SetType("ns3::AdhocWifiMac");
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("DsssRate1Mbps"),
                               "ControlMode", StringValue("DsssRate1Mbps"));

  wifiPhy.Set("ChannelNumber", UintegerValue(1));

  m_devices = wifi.Install(wifiPhy, wifiMac, m_nodes);
}

void
WifiPerformanceExperiment::SetupApplications()
{
  UdpEchoServerHelper echoServer(9);
  m_apps = echoServer.Install(m_nodes.Get(1));
  m_apps.Start(Seconds(1.0));
  m_apps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(m_interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  m_apps = echoClient.Install(m_nodes.Get(0));
  m_apps.Start(Seconds(2.0));
  m_apps.Stop(Seconds(10.0));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Rx", MakeCallback(&ReceiveCallback, this));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback(&TransmitCallback, this));
}

void
WifiPerformanceExperiment::SetupStats()
{
  if (format == "omnet")
    {
      AsciiDataOutput ascii;
      ascii.Output(m_dataCollector, "wifi-performance-omnet.out");
    }
  else if (format == "sqlite")
    {
      SqliteDataOutput sqlite;
      sqlite.SetOutputFilename("wifi-performance-sqlite.db");
      sqlite.Output(m_dataCollector);
    }

  m_dataCollector.AddData("Packets", "Tx", 0);
  m_dataCollector.AddData("Packets", "Rx", 0);
}

void
WifiPerformanceExperiment::Run(uint32_t distance, std::string format, uint32_t runId)
{
  SetupNodes();
  SetupWifi();
  SetupMobility();
  SetupApplications();
  SetupStats();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  uint32_t distance = 10;
  std::string format = "omnet";
  uint32_t runId = 1;

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("format", "Output format: 'omnet' or 'sqlite'", format);
  cmd.AddValue("runId", "Run identifier for the experiment", runId);
  cmd.Parse(argc, argv);

  WifiPerformanceExperiment experiment;
  experiment.Run(distance, format, runId);

  return 0;
}