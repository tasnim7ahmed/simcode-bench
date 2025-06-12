#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpOver80211n");

class Tcp80211nExperiment
{
public:
  Tcp80211nExperiment();
  void Run(const std::string& tcpVersion,
           const std::string& rate,
           uint32_t payloadSize,
           uint32_t physicalMode,
           double simulationTime,
           bool pcapTracing);

private:
  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
};

Tcp80211nExperiment::Tcp80211nExperiment()
{
  wifiStaNode.Create(1);
  wifiApNode.Create(1);
}

void ThroughputMonitor(Ptr<FlowMonitor> monitor, FlowId flowId)
{
  double throughput = 0;
  Time now = Simulator::Now();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(monitor->GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  if (stats.find(flowId) != stats.end())
  {
    auto d = stats[flowId];
    throughput = (d.rxBytes * 8.0) / (now.GetSeconds() - d.timeLastRxPacket.GetSeconds()) / 1000 / 1000; // Mbps
  }

  std::cout << now.As(Time::S) << "\t" << throughput << std::endl;

  Simulator::Schedule(MilliSeconds(100), &ThroughputMonitor, monitor, flowId);
}

void Tcp80211nExperiment::Run(const std::string& tcpVersion,
                              const std::string& rate,
                              uint32_t payloadSize,
                              uint32_t physicalMode,
                              double simulationTime,
                              bool pcapTracing)
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(tcpVersion)));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(999999));
  Config::SetDefault("ns3::WifiMacQueueItem::MaxPacketCount", UintegerValue(1000));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(std::to_string(physicalMode)),
                               "ControlMode", StringValue(std::to_string(physicalMode)));

  WifiMacHelper mac;
  Ssid ssid = Ssid("tcp-80211n");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true));
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
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
  staInterfaces = address.Assign(staDevices);
  apInterface = address.Assign(apDevice);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(wifiApNode.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simulationTime + 0.1));

  BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute("MaxBytes", UintegerValue(0));
  source.SetAttribute("SendSize", UintegerValue(payloadSize));
  ApplicationContainer sourceApps = source.Install(wifiStaNode.Get(0));
  sourceApps.Start(Seconds(0.1));
  sourceApps.Stop(Seconds(simulationTime + 0.1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (pcapTracing)
  {
    phy.EnablePcap("TcpOver80211n", apDevice.Get(0));
  }

  Simulator::Schedule(Seconds(0.1), &ThroughputMonitor, monitor, 1);

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char* argv[])
{
  std::string tcpVersion = "TcpNewReno";
  std::string dataRate = "100Mbps";
  uint32_t payloadSize = 1448;
  uint32_t physicalMode = 6; // MCS index for 802.11n
  double simulationTime = 10;
  bool pcapTracing = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("tcpVersion", "TCP congestion control algorithm: TcpNewReno, TcpCubic, etc.", tcpVersion);
  cmd.AddValue("dataRate", "Application data rate (e.g., 100Mbps)", dataRate);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("physicalMode", "Wi-Fi MCS index (e.g., 6)", physicalMode);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("pcapTracing", "Enable PCAP tracing", pcapTracing);
  cmd.Parse(argc, argv);

  Tcp80211nExperiment experiment;
  experiment.Run(tcpVersion, dataRate, payloadSize, physicalMode, simulationTime, pcapTracing);

  return 0;
}