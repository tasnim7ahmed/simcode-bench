#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosThroughputSimulation");

class WifiTosThroughputExperiment
{
public:
  WifiTosThroughputExperiment();
  void Run(uint32_t nStations, uint8_t mcsValue, uint16_t channelWidth, bool shortGuardInterval,
           double distance, bool enableRtsCts, Time simulationTime);

private:
  void SetupNetwork(uint32_t nStations, uint8_t mcsValue, uint16_t channelWidth, bool shortGuardInterval,
                    double distance, bool enableRtsCts);
  void InstallApplications();
  void CalculateThroughput();

  NodeContainer wifiStaNodes;
  NodeContainer wifiApNode;
  NetDeviceContainer staDevs;
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  UdpServerHelper udpServer;
  std::vector<UdpClientHelper> udpClients;
  Time m_simulationTime;
};

WifiTosThroughputExperiment::WifiTosThroughputExperiment()
{
  wifiApNode.Create(1);
  udpServer = UdpServerHelper(9);
}

void
WifiTosThroughputExperiment::Run(uint32_t nStations, uint8_t mcsValue, uint16_t channelWidth,
                                 bool shortGuardInterval, double distance, bool enableRtsCts,
                                 Time simulationTime)
{
  m_simulationTime = simulationTime;

  SetupNetwork(nStations, mcsValue, channelWidth, shortGuardInterval, distance, enableRtsCts);
  InstallApplications();

  Simulator::Stop(simulationTime);
  Simulator::Run();
  CalculateThroughput();
  Simulator::Destroy();
}

void
WifiTosThroughputExperiment::SetupNetwork(uint32_t nStations, uint8_t mcsValue, uint16_t channelWidth,
                                         bool shortGuardInterval, double distance,
                                         bool enableRtsCts)
{
  wifiStaNodes.Create(nStations);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("HtMcs" + std::to_string(mcsValue)),
                               "ControlMode", StringValue("HtMcs" + std::to_string(mcsValue)));

  if (enableRtsCts)
    {
      wifi.Set("RtsCtsThreshold", UintegerValue(1));
    }
  else
    {
      wifi.Set("RtsCtsThreshold", UintegerValue(UINT32_MAX));
    }

  phy.Set("ChannelWidth", UintegerValue(channelWidth));
  phy.Set("ShortGuardInterval", BooleanValue(shortGuardInterval));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(Ssid("ns3-ssid")),
              "ActiveProbing", BooleanValue(false));
  staDevs = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(Ssid("ns3-ssid")));
  NetDeviceContainer apDev = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0),
                                "GridWidth", UintegerValue(nStations),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  apInterface = address.Assign(apDev);
  staInterfaces = address.Assign(staDevs);
}

void
WifiTosThroughputExperiment::InstallApplications()
{
  ApplicationContainer serverApps = udpServer.Install(wifiApNode.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(m_simulationTime);

  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
      UdpClientHelper client(staInterfaces.GetAddress(i), 9);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(1024));
      client.SetAttribute("Priority", UintegerValue(i % 8)); // Vary TOS values

      ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(m_simulationTime - Seconds(0.1));
      udpClients.push_back(client);
    }
}

void
WifiTosThroughputExperiment::CalculateThroughput()
{
  double totalRxBytes = 0;
  auto serverApp = DynamicCast<UdpServer>(udpServer.GetServer());
  if (serverApp)
    {
      totalRxBytes = serverApp->GetReceived();
    }

  double throughput = (totalRxBytes * 8) / (m_simulationTime.GetSeconds() * 1000000); // Mbps
  std::cout.precision(3);
  std::cout << "Aggregated Throughput: " << std::fixed << throughput << " Mbps" << std::endl;
}

int
main(int argc, char* argv[])
{
  uint32_t nStations = 5;
  uint8_t mcsValue = 7;
  uint16_t channelWidth = 40;
  bool shortGuardInterval = true;
  double distance = 10.0;
  bool enableRtsCts = false;
  Time simulationTime = Seconds(10.0);

  CommandLine cmd(__FILE__);
  cmd.AddValue("nStations", "Number of stations", nStations);
  cmd.AddValue("mcsValue", "HT MCS value (0 to 7)", mcsValue);
  cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
  cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
  cmd.AddValue("distance", "Distance between stations and AP", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.Parse(argc, argv);

  NS_ABORT_MSG_IF(mcsValue > 7, "Invalid MCS value. Must be between 0 and 7.");
  NS_ABORT_MSG_IF(channelWidth != 20 && channelWidth != 40,
                  "Channel width must be either 20 or 40 MHz.");

  RngSeedManager::SetSeed(1);
  RngSeedManager::SetRun(1);

  WifiTosThroughputExperiment experiment;
  experiment.Run(nStations, mcsValue, channelWidth, shortGuardInterval, distance, enableRtsCts,
                 simulationTime);

  return 0;
}