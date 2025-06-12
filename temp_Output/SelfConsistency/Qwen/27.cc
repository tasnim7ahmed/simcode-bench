#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsGoodputExperiment");

class WifiGoodputExperiment
{
public:
  WifiGoodputExperiment();
  void Run();

private:
  void SetupSimulation(uint32_t mcsValue, uint16_t channelWidth, bool shortGuardInterval,
                       Time simulationTime, bool useTcp, bool enableRtsCts, double distance,
                       DataRate expectedThroughput);

  NodeContainer m_staNodes;
  NodeContainer m_apNode;
};

WifiGoodputExperiment::WifiGoodputExperiment()
{
  m_staNodes.Create(1);
  m_apNode.Create(1);
}

void
WifiGoodputExperiment::SetupSimulation(uint32_t mcsValue, uint16_t channelWidth,
                                       bool shortGuardInterval, Time simulationTime,
                                       bool useTcp, bool enableRtsCts, double distance,
                                       DataRate expectedThroughput)
{
  // Mobility setup
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_apNode);
  mobility.Install(m_staNodes);

  // WiFi setup
  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  // Set default rates and channel width
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(mcsValue)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(mcsValue)));

  if (enableRtsCts)
    {
      wifiMac.SetAttribute("RTSThreshold", UintegerValue(1));
    }
  else
    {
      wifiMac.SetAttribute("RTSThreshold", UintegerValue(UINT_MAX));
    }

  // Channel configuration
  Config::SetDefault("ns3::WifiPhy::ChannelWidth", UintegerValue(channelWidth));
  Config::SetDefault("ns3::WifiPhy::ShortGuardInterval", BooleanValue(shortGuardInterval));

  // Install PHY and MAC layers
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(Ssid("wifi-goodput")),
                  "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, wifiMac, m_staNodes);

  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(Ssid("wifi-goodput")));
  NetDeviceContainer apDevices = wifi.Install(phy, wifiMac, m_apNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(m_staNodes);
  stack.Install(m_apNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

  // Application setup
  uint16_t port = 9;
  if (useTcp)
    {
      BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
      source.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

      ApplicationContainer sourceApp = source.Install(m_apNode.Get(0));
      sourceApp.Start(Seconds(0.0));
      sourceApp.Stop(simulationTime - Seconds(0.1));

      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(m_staNodes.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(simulationTime);
    }
  else
    {
      OnOffHelper source("ns3::UdpSocketFactory", InetSocketAddress(staInterface.GetAddress(0), port));
      source.SetAttribute("PacketSize", UintegerValue(1472));
      source.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      source.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      source.SetAttribute("DataRate", DataRateValue(expectedThroughput));

      ApplicationContainer sourceApp = source.Install(m_apNode.Get(0));
      sourceApp.Start(Seconds(0.0));
      sourceApp.Stop(simulationTime - Seconds(0.1));

      PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(m_staNodes.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(simulationTime);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(simulationTime);
  Simulator::Run();

  monitor->CheckForLostPackets();

  FlowMonitor::FlowStats stats = monitor->GetFlowStats()[*(monitor->GetFlowStats().begin())];

  double throughput = stats.rxBytes * 8.0 / simulationTime.GetSeconds().GetValue() / 1e6; // Mbps

  NS_LOG_UNCOND("Configuration: MCS=" << mcsValue
                                     << ", Width=" << channelWidth << "MHz"
                                     << ", GI=" << (shortGuardInterval ? "Short" : "Long")
                                     << ", Distance=" << distance << "m"
                                     << ", Protocol=" << (useTcp ? "TCP" : "UDP")
                                     << ", Throughput=" << throughput << " Mbps"
                                     << ", Expected=" << expectedThroughput.GetBitRate() / 1e6 << " Mbps"
                                     << ", Match=" << ((throughput >= expectedThroughput.GetBitRate() / 1e6 * 0.9) ? "Yes" : "No"));

  Simulator::Destroy();
}

void
WifiGoodputExperiment::Run()
{
  CommandLine cmd(__FILE__);
  uint32_t mcsValue = 0;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = true;
  double distance = 1.0;
  bool useTcp = false;
  bool enableRtsCts = false;
  double simulationTime = 10.0;
  DataRate expectedThroughput("100Mbps");

  cmd.AddValue("mcs", "Modulation and Coding Scheme (0-7)", mcsValue);
  cmd.AddValue("width", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue("sgi", "Use Short Guard Interval", shortGuardInterval);
  cmd.AddValue("distance", "Distance between station and AP (in meters)", distance);
  cmd.AddValue("tcp", "Use TCP instead of UDP", useTcp);
  cmd.AddValue("rtscts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue("time", "Simulation time in seconds", simulationTime);
  cmd.AddValue("expected", "Expected throughput (e.g., 100Mbps)", expectedThroughput);

  cmd.Parse(Simulator::Get argc, Simulator::Get argv);

  SetupSimulation(mcsValue, channelWidth, shortGuardInterval, Seconds(simulationTime),
                  useTcp, enableRtsCts, distance, expectedThroughput);
}

int
main(int argc, char* argv[])
{
  WifiGoodputExperiment experiment;
  experiment.Run();
  return 0;
}