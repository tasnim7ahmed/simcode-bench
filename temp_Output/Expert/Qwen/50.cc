#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsThroughputSimulation");

class WifiMcsThroughputHelper : public Object
{
public:
  static TypeId GetTypeId(void);
  WifiMcsThroughputHelper();
  void Setup(Ptr<Node> apNode, Ptr<Node> staNode, double distance, uint16_t channelWidth, WifiStandard standard, bool useShortGuardInterval);
};

TypeId
WifiMcsThroughputHelper::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::WifiMcsThroughputHelper")
    .SetParent<Object>()
    .AddConstructor<WifiMcsThroughputHelper>()
    ;
  return tid;
}

WifiMcsThroughputHelper::WifiMcsThroughputHelper()
{
}

void
WifiMcsThroughputHelper::Setup(Ptr<Node> apNode, Ptr<Node> staNode, double distance, uint16_t channelWidth, WifiStandard standard, bool useShortGuardInterval)
{
  // Create wifi helper
  WifiHelper wifi;
  wifi.SetStandard(standard);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate24Mbps"), "ControlMode", StringValue("OfdmRate24Mbps"));

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-mcs-throughput");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install(phy, mac, staNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, apNode);

  // Set channel width and guard interval
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(channelWidth));
  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue(useShortGuardInterval));

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(staNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(apNode);
  stack.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  // UDP server (on STA)
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(staNode);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP client (on AP)
  UdpClientHelper client(staInterface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
  client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
  client.SetAttribute("PacketSize", UintegerValue(1400));
  ApplicationContainer clientApps = client.Install(apNode);
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // Enable pcap tracing
  phy.EnablePcap("wifi-mcs-throughput", apDevice.Get(0));
}

int main(int argc, char *argv[])
{
  double distance = 10.0;
  uint16_t channelWidth = 20;
  uint32_t simulationTime = 10;
  std::string phyType = "ns3::YansWifiPhy";
  std::string errorModelType = "ns3::NistErrorRateModel";
  bool useShortGuardInterval = false;
  WifiStandard standard = WIFI_STANDARD_80211ac;

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance in meters between the two nodes", distance);
  cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("phyType", "PHY type: Yans or Spectrum", phyType);
  cmd.AddValue("errorModelType", "Error model: Nist or other", errorModelType);
  cmd.AddValue("useShortGuardInterval", "Use short guard interval", useShortGuardInterval);
  cmd.AddValue("standard", "Wi-Fi standard (e.g., 802.11ac)", standard);
  cmd.Parse(argc, argv);

  // Global values
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate24Mbps"));
  Config::SetDefault("ns3::WifiPhy::ErrorRateModel", TypeIdValue(ErrorRateModel::GetTypeId().LookupByName(errorModelType)));

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);
  Ptr<Node> apNode = nodes.Get(0);
  Ptr<Node> staNode = nodes.Get(1);

  WifiMcsThroughputHelper helper;
  helper.Setup(apNode, staNode, distance, channelWidth, standard, useShortGuardInterval);

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}