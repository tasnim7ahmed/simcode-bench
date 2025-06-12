#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessNetwork");

class MixedNetworkSimulator {
public:
  MixedNetworkSimulator();
  void Run(int argc, char *argv[]);
private:
  uint32_t m_numBackboneRouters;
  uint32_t m_nodesPerLan;
  uint32_t m_wirelessStasPerAp;
  bool m_traceMobility;
  bool m_generateTrace;
  NodeContainer m_backboneNodes;
  NetDeviceContainer m_backboneDevices;
  Ipv4InterfaceContainer m_backboneInterfaces;
  std::vector<NodeContainer> m_lanNodes;
  std::vector<NetDeviceContainer> m_lanDevices;
  std::vector<Ipv4InterfaceContainer> m_lanInterfaces;
  std::vector<Ptr<WifiApHelper>> m_apHelpers;
  std::vector<WifiHelper> m_wifiHelpers;
  std::vector<YansWifiPhyHelper> m_phyHelpers;
  std::vector<NqosWifiMacHelper> m_macHelpers;
  CsmaHelper m_csmaHelper;
  PointToPointHelper m_p2pHelper;
  OlsrHelper m_olsr;
  CommandLine m_cmd;

  void SetupCommandLine();
  void CreateNodes();
  void SetupMobility();
  void SetupNetwork();
  void SetupInternetStack();
  void SetupApplications();
  void EnableTracing();
  void CourseChangeCallback(std::string context, const MobilityModel *model);
};

MixedNetworkSimulator::MixedNetworkSimulator()
    : m_numBackboneRouters(3),
      m_nodesPerLan(2),
      m_wirelessStasPerAp(1),
      m_traceMobility(false),
      m_generateTrace(false)
{
  m_cmd.AddValue("numBackboneRouters", "Number of backbone routers in the ad hoc WiFi network", m_numBackboneRouters);
  m_cmd.AddValue("nodesPerLan", "Number of LAN nodes per backbone router", m_nodesPerLan);
  m_cmd.AddValue("wirelessStasPerAp", "Number of wireless stations per access point", m_wirelessStasPerAp);
  m_cmd.AddValue("traceMobility", "Enable mobility tracing", m_traceMobility);
  m_cmd.AddValue("generateTrace", "Generate packet trace files", m_generateTrace);
}

void MixedNetworkSimulator::SetupCommandLine() {
  m_cmd.Parse(argc, argv);
}

void MixedNetworkSimulator::CreateNodes() {
  // Create backbone routers (mobile nodes with WiFi and LAN interfaces)
  m_backboneNodes.Create(m_numBackboneRouters);

  // Create LAN nodes for each backbone router
  m_lanNodes.resize(m_numBackboneRouters);
  for (uint32_t i = 0; i < m_numBackboneRouters; ++i) {
    m_lanNodes[i].Create(m_nodesPerLan);
  }
}

void MixedNetworkSimulator::SetupMobility() {
  if (m_traceMobility) {
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/CourseChange",
                    MakeCallback(&MixedNetworkSimulator::CourseChangeCallback, this));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  mobility.Install(m_backboneNodes);

  // Install fixed position mobility for LAN nodes
  MobilityHelper lanMobility;
  lanMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  for (uint32_t i = 0; i < m_numBackboneRouters; ++i) {
    lanMobility.Install(m_lanNodes[i]);
  }
}

void MixedNetworkSimulator::SetupNetwork() {
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  WifiHelper wifi = WifiHelper::Default();
  wifi.SetStandard(WIFI_STANDARD_80211a);
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();

  // Configure WiFi for backbone mesh
  wifiPhy.SetChannelNumber(36);
  wifi.SetRemoteStationManager("ns3::AarfcdWifiManager");
  wifiMac.SetType("ns3::AdhocWifiMac");
  m_backboneDevices = wifi.Install(wifiPhy, wifiMac, m_backboneNodes);

  // Configure CSMA for LANs
  m_csmaHelper.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  m_csmaHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  // Configure WiFi for infrastructure networks
  for (uint32_t i = 0; i < m_numBackboneRouters; ++i) {
    // Create new helpers for each AP-STA group to avoid interference
    m_apHelpers.push_back(Ptr<WifiApHelper>(new WifiApHelper()));
    m_wifiHelpers.push_back(WifiHelper());
    m_phyHelpers.push_back(YansWifiPhyHelper::Default());
    m_macHelpers.push_back(NqosWifiMacHelper::Default());

    // Configure infrastructure WiFi
    m_wifiHelpers[i].SetStandard(WIFI_STANDARD_80211a);
    m_wifiHelpers[i].SetRemoteStationManager("ns3::AarfcdWifiManager");
    m_phyHelpers[i].SetChannelNumber(40 + (i % 4) * 4); // Different channels for different APs

    // Create AP
    m_apHelpers[i]->SetPhy(m_phyHelpers[i]);
    m_apHelpers[i]->SetMac(m_macHelpers[i], "Ssid", SsidValue(Ssid("wifi-network-" + std::to_string(i))));
    m_apHelpers[i]->SetMac("QosSupported", BooleanValue(true));

    // Install AP on corresponding backbone node
    NetDeviceContainer apDevice = m_apHelpers[i]->Install(m_backboneNodes.Get(i), m_phyHelpers[i], wifiMac);

    // Install STAs
    m_wifiHelpers[i].SetPhy(m_phyHelpers[i]);
    m_wifiHelpers[i].SetMac(m_macHelpers[i], "Ssid", SsidValue(Ssid("wifi-network-" + std::to_string(i))));
    NetDeviceContainer staDevices = m_wifiHelpers[i].Install(m_lanNodes[i]);

    // Connect all devices
    NetDeviceContainer totalDevices;
    totalDevices.Add(apDevice);
    totalDevices.Add(staDevices);
    m_lanDevices.push_back(totalDevices);
  }

  // Connect backbone nodes with point-to-point links for initial connectivity
  m_p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  m_p2pHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  for (uint32_t i = 0; i < m_numBackboneRouters - 1; ++i) {
    NodeContainer pair(m_backboneNodes.Get(i), m_backboneNodes.Get(i+1));
    NetDeviceContainer p2pDevices = m_p2pHelper.Install(pair);
    m_backboneDevices.Add(p2pDevices);
  }
}

void MixedNetworkSimulator::SetupInternetStack() {
  InternetStackHelper stack;
  stack.SetRoutingHelper(m_olsr); // Use OLSR routing protocol for backbone

  // Install internet stacks
  stack.Install(m_backboneNodes);
  for (uint32_t i = 0; i < m_numBackboneRouters; ++i) {
    stack.Install(m_lanNodes[i]);
  }

  // Assign IP addresses
  Ipv4AddressHelper ip;
  ip.SetBase("10.1.0.0", "255.255.255.0");
  m_backboneInterfaces = ip.Assign(m_backboneDevices);

  ip.SetBase("10.2.0.0", "255.255.0.0");
  for (uint32_t i = 0; i < m_numBackboneRouters; ++i) {
    ip.NewNetwork();
    m_lanInterfaces.push_back(ip.Assign(m_lanDevices[i]));
  }
}

void MixedNetworkSimulator::SetupApplications() {
  // Set up UDP flow from first LAN node to last STA
  uint32_t destLanIndex = m_numBackboneRouters - 1;
  uint32_t destStaIndex = m_wirelessStasPerAp > 0 ? 1 : 0; // First device is AP, then STAs

  Address sinkAddress(InetSocketAddress(m_lanInterfaces[destLanIndex].GetAddress(destStaIndex), 9));
  PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApps = packetSinkHelper.Install(m_lanNodes[destLanIndex].Get(destStaIndex));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(10.0));

  OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetConstantRate(DataRate("1Mbps"), 512);
  ApplicationContainer sourceApp = onoff.Install(m_lanNodes[0].Get(0)); // First LAN node
  sourceApp.Start(Seconds(2.0));
  sourceApp.Stop(Seconds(9.0));
}

void MixedNetworkSimulator::EnableTracing() {
  if (m_generateTrace) {
    AsciiTraceHelper asciiTraceHelper;
    wifiPhy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("mixed-network.tr"));
    m_p2pHelper.EnablePcapAll("mixed-network");
    
    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    monitor->SerializeToXmlFile("mixed-network.flowmon", false, false);
  }
}

void MixedNetworkSimulator::CourseChangeCallback(std::string context, const MobilityModel *model) {
  Vector position = model->GetPosition();
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s " << context 
                  << " moved to (" << position.x << "," << position.y << ")");
}

void MixedNetworkSimulator::Run(int argc, char *argv[]) {
  SetupCommandLine();
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Creating nodes.");
  CreateNodes();

  NS_LOG_INFO("Setting up mobility.");
  SetupMobility();

  NS_LOG_INFO("Setting up network topology.");
  SetupNetwork();

  NS_LOG_INFO("Installing internet stack with OLSR routing.");
  SetupInternetStack();

  NS_LOG_INFO("Setting up applications.");
  SetupApplications();

  if (m_generateTrace || m_traceMobility) {
    NS_LOG_INFO("Enabling traces.");
    EnableTracing();
  }

  NS_LOG_INFO("Starting simulation.");
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  MixedNetworkSimulator sim;
  sim.Run(argc, argv);
  return 0;
}