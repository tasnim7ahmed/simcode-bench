#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/gnuplot.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughput");

class WifiMimoThroughputExperiment
{
public:
  WifiMimoThroughputExperiment();
  void Run(uint32_t mcs, uint32_t nStreams, double distance);
  void Throughput();

private:
  NodeContainer wifiStaNode;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevices;
  NetDeviceContainer apDevice;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  Gnuplot2dDataset dataset;
  uint32_t payloadSize;
  uint32_t m_port;
};

WifiMimoThroughputExperiment::WifiMimoThroughputExperiment()
  : payloadSize(1472),
    m_port(9)
{
  wifiStaNode.Create(1);
  wifiApNode.Create(1);

  // Mobility setup
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP at origin
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);
}

void
WifiMimoThroughputExperiment::Run(uint32_t mcs, uint32_t nStreams, double distance)
{
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HTMCS" + std::to_string(mcs)),
                               "ControlMode", StringValue("HTMCS" + std::to_string(mcs)));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("mimo-network")));
  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("mimo-network")));
  apDevice = wifi.Install(phy, mac, wifiApNode);

  // Update station position
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(distance, 0.0, 0.0));
  MobilityHelper mobility;
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode.Get(0));

  // Install IP interfaces
  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  staInterfaces = address.Assign(staDevices);
  apInterface = address.Assign(apDevice);

  // Setup applications
  UdpServerHelper server(m_port);
  ApplicationContainer sinkApp = server.Install(wifiApNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  UdpClientHelper client(apInterface.GetAddress(0), m_port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
  client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  client.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Calculate throughput
  uint64_t totalRxBytes = DynamicCast<UdpServer>(sinkApp.Get(0))->GetReceived();
  double throughput = (totalRxBytes * 8) / (9.0 * 1e6); // Mbps

  dataset.Add(distance, throughput);

  Simulator::Destroy();
}

void
WifiMimoThroughputExperiment::Throughput()
{
  Gnuplot gnuplot("wifi-mimo-throughput.png");
  gnuplot.SetTitle("Wi-Fi MIMO Throughput vs Distance");
  gnuplot.SetTerminal("png");
  gnuplot.SetLegend("Distance (meters)", "Throughput (Mbps)");
  gnuplot.SetExtra("set key top left box");

  for (uint32_t streams = 1; streams <= 4; ++streams)
  {
    dataset.Clear();
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    dataset.SetTitle("MIMO Streams: " + std::to_string(streams));

    for (uint32_t mcs = 0; mcs < 32; ++mcs) // HT MCS ranges from 0 to 31
    {
      Run(mcs, streams, 10.0); // fixed distance for testing in this example
    }

    gnuplot.AddDataset(dataset);
  }

  std::ofstream plotFile("wifi-mimo-throughput.plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();
}

int
main(int argc, char* argv[])
{
  uint32_t simulationTime = 10;
  bool enableTcp = false;
  bool enableUdp = true;
  bool use5Ghz = true;
  uint32_t channelWidth = 40;
  bool shortGuardInterval = true;
  bool preambleDetection = true;
  uint32_t stepSize = 5;
  bool channelBonding = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("enableTcp", "Enable TCP traffic", enableTcp);
  cmd.AddValue("enableUdp", "Enable UDP traffic", enableUdp);
  cmd.AddValue("use5Ghz", "Use 5 GHz frequency band", use5Ghz);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
  cmd.AddValue("preambleDetection", "Enable preamble detection", preambleDetection);
  cmd.AddValue("stepSize", "Distance step size (m)", stepSize);
  cmd.AddValue("channelBonding", "Enable channel bonding", channelBonding);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));

  if (!enableTcp && !enableUdp)
  {
    NS_FATAL_ERROR("At least one of TCP or UDP must be enabled.");
  }

  WifiMimoThroughputExperiment experiment;
  experiment.Throughput();

  return 0;
}