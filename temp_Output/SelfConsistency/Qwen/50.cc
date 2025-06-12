#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMcsThroughputTest");

class McsThroughputExperiment
{
public:
  McsThroughputExperiment();
  void Run(uint16_t channelWidth, uint8_t mcsValue, double distance, std::string phyMode, std::string errorModelType);
  void PrintStats();

private:
  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  uint32_t rxPackets;
};

McsThroughputExperiment::McsThroughputExperiment()
{
  rxPackets = 0;
}

void
McsThroughputExperiment::Run(uint16_t channelWidth, uint8_t mcsValue, double distance,
                             std::string phyMode, std::string errorModelType)
{
  // Create nodes
  wifiStaNode.Create(1);
  wifiApNode.Create(1);

  // Setup mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // AP
  positionAlloc->Add(Vector(distance, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  // Setup Wi-Fi
  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);

  // Set error model
  if (errorModelType == "nistsea")
  {
    wifi.SetErrorRateModel("ns3::NistErrorRateModel");
  }
  else if (errorModelType == "yans")
  {
    wifi.SetErrorRateModel("ns3::YansErrorRateModel");
  }

  // Use YansWifiPhy by default
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  // Configure Wi-Fi PHY
  wifiPhy.Set("ChannelWidth", UintegerValue(channelWidth));
  wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
  wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
  wifiPhy.Set("TxPowerLevels", UintegerValue(1));

  // Configure Wi-Fi MAC
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  staDevices = wifi.Install(wifiPhy, wifiMac, wifiStaNode.Get(0));

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  apDevice = wifi.Install(wifiPhy, wifiMac, wifiApNode.Get(0));

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  apInterface = address.Assign(apDevice);
  staInterfaces = address.Assign(staDevices);

  // UDP traffic: sink on AP, source on STA
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer sinkApps = server.Install(wifiApNode.Get(0));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  UdpClientHelper client(staInterfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
  client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  client.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer clientApps = client.Install(wifiStaNode.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // Flow monitor to calculate throughput
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Gather statistics
  monitor->CheckForLostPackets();
  FlowMonitor::FlowStats stats = monitor->GetFlowStats()[clientApps.Get(0)->GetId()];
  rxPackets = stats.rxPackets;

  Simulator::Destroy();
}

void
McsThroughputExperiment::PrintStats()
{
  std::cout << "Received Packets: " << rxPackets << std::endl;
}

int
main(int argc, char* argv[])
{
  double distance = 10.0;          // meters
  double simulationTime = 10.0;    // seconds
  uint16_t channelWidth = 20;
  uint8_t mcsValue = 7;
  std::string phyMode = "DsssRate1Mbps";
  std::string phyType = "Yans";
  std::string errorModelType = "nistsea";

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance between nodes (meters)", distance);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("mcsValue", "MCS value (0-7 for 802.11ac)", mcsValue);
  cmd.AddValue("phyType", "PHY type: Yans or Spectrum", phyType);
  cmd.AddValue("errorModelType", "Error model: nistsea or yans", errorModelType);
  cmd.Parse(argc, argv);

  McsThroughputExperiment experiment;
  experiment.Run(channelWidth, mcsValue, distance, phyMode, errorModelType);
  experiment.PrintStats();

  return 0;
}