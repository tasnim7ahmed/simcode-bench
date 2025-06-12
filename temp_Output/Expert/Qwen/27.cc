#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGoodputSimulation");

class WifiGoodputExperiment
{
public:
  WifiGoodputExperiment();
  void Run(uint16_t channelWidth, uint8_t mcsValue, bool shortGuardInterval, bool enableRtsCts,
           std::string transportMode, double distance, double simulationTime, double expectedThroughput);

private:
  void SetupNetwork(NodeContainer &wifiStaNode, NodeContainer &wifiApNode, NetDeviceContainer &staDevices,
                    NetDeviceContainer &apDevice, Ipv4InterfaceContainer &staInterfaces, Ipv4InterfaceContainer &apInterface,
                    uint16_t channelWidth, uint8_t mcsValue, bool shortGuardInterval, bool enableRtsCts, double distance);
  void SetupApplications(NodeContainer &wifiStaNode, NodeContainer &wifiApNode, Ipv4InterfaceContainer &staInterfaces,
                         Ipv4InterfaceContainer &apInterface, std::string transportMode, double simulationTime,
                         FlowMonitorHelper &flowmon, Ptr<FlowMonitor> &monitor, double expectedThroughput);
};

WifiGoodputExperiment::WifiGoodputExperiment()
{
}

void WifiGoodputExperiment::Run(uint16_t channelWidth, uint8_t mcsValue, bool shortGuardInterval, bool enableRtsCts,
                                std::string transportMode, double distance, double simulationTime,
                                double expectedThroughput)
{
  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HTMIX-" + std::to_string(mcsValue)),
                               "ControlMode", StringValue("HTMIX-" + std::to_string(mcsValue)));

  if (enableRtsCts)
    {
      mac.SetAttribute("RTSThreshold", UintegerValue(1));
    }

  phy.Set("ChannelWidth", UintegerValue(channelWidth));
  phy.Set("ShortGuardIntervalEnabled", BooleanValue(shortGuardInterval));

  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;

  Ssid ssid = Ssid("wifi-goodput");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  SetupApplications(wifiStaNode, wifiApNode, staInterfaces, apInterface, transportMode, simulationTime, flowmon, monitor,
                    expectedThroughput);

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();

  double throughput = 0.0;
  uint64_t totalRxBytes = 0;

  for (auto flow : monitor->GetFlowStats())
    {
      if (flow.first.m_source == staInterfaces.GetAddress(0) && flow.first.m_destination == apInterface.GetAddress(0))
        {
          totalRxBytes += flow.second.rxBytes;
        }
    }

  throughput = (totalRxBytes * 8.0) / simulationTime / 1e6;

  NS_LOG_UNCOND("Configuration: MCS=" << (uint32_t)mcsValue
                                      << ", ChannelWidth=" << channelWidth << "MHz"
                                      << ", GuardInterval=" << (shortGuardInterval ? "Short" : "Long")
                                      << ", Transport=" << transportMode
                                      << ", RTS/CTS=" << (enableRtsCts ? "Enabled" : "Disabled")
                                      << ", Distance=" << distance << "m"
                                      << ", Throughput=" << throughput << " Mbps"
                                      << ", Expected=" << expectedThroughput << " Mbps");

  Simulator::Destroy();
}

void WifiGoodputExperiment::SetupNetwork(NodeContainer &wifiStaNode, NodeContainer &wifiApNode,
                                         NetDeviceContainer &staDevices, NetDeviceContainer &apDevice,
                                         Ipv4InterfaceContainer &staInterfaces, Ipv4InterfaceContainer &apInterface,
                                         uint16_t channelWidth, uint8_t mcsValue, bool shortGuardInterval,
                                         bool enableRtsCts, double distance)
{
}

void WifiGoodputExperiment::SetupApplications(NodeContainer &wifiStaNode, NodeContainer &wifiApNode,
                                              Ipv4InterfaceContainer &staInterfaces,
                                              Ipv4InterfaceContainer &apInterface, std::string transportMode,
                                              double simulationTime, FlowMonitorHelper &flowmon,
                                              Ptr<FlowMonitor> &monitor, double expectedThroughput)
{
  uint16_t port = 9;

  if (transportMode == "UDP")
    {
      OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate(expectedThroughput * 1e6)));
      onoff.SetAttribute("PacketSize", UintegerValue(1472));

      ApplicationContainer apps = onoff.Install(wifiStaNode.Get(0));
      apps.Start(Seconds(0.0));
      apps.Stop(Seconds(simulationTime - 1));
    }
  else if (transportMode == "TCP")
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(simulationTime));

      OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute("DataRate", DataRateValue(DataRate(expectedThroughput * 1e6)));
      onoff.SetAttribute("PacketSize", UintegerValue(1448));

      ApplicationContainer apps = onoff.Install(wifiStaNode.Get(0));
      apps.Start(Seconds(0.0));
      apps.Stop(Seconds(simulationTime - 1));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

int main(int argc, char *argv[])
{
  uint16_t channelWidth = 20;
  uint8_t mcsValue = 0;
  bool shortGuardInterval = false;
  bool enableRtsCts = false;
  std::string transportMode = "UDP";
  double distance = 1.0;
  double simulationTime = 10.0;
  double expectedThroughput = 50.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue("mcsValue", "Modulation and Coding Scheme value (0 to 7)", mcsValue);
  cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake (true/false)", enableRtsCts);
  cmd.AddValue("transportMode", "Transport protocol (UDP or TCP)", transportMode);
  cmd.AddValue("distance", "Distance between station and AP (in meters)", distance);
  cmd.AddValue("simulationTime", "Duration of the simulation in seconds", simulationTime);
  cmd.AddValue("expectedThroughput", "Expected throughput in Mbps", expectedThroughput);
  cmd.Parse(argc, argv);

  if (mcsValue > 7 || (channelWidth != 20 && channelWidth != 40))
    {
      NS_ABORT_MSG("Invalid MCS value or channel width.");
    }

  WifiGoodputExperiment experiment;
  experiment.Run(channelWidth, mcsValue, shortGuardInterval, enableRtsCts, transportMode, distance, simulationTime,
                 expectedThroughput);

  return 0;
}