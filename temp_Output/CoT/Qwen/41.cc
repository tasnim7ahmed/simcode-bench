#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessGridSimulation");

class GridSimulation {
public:
  GridSimulation();
  void Setup(int argc, char *argv[]);
  void Run();

private:
  uint32_t m_gridSize;
  double m_gridDistance;
  std::string m_phyMode;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  uint32_t m_sourceNode;
  uint32_t m_sinkNode;
  bool m_verbose;
  bool m_pcapTracing;
  bool m_asciiTracing;
  double m_rssiCutoff;

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;

  void CreateNodes();
  void CreateDevices();
  void InstallApplications();
};

GridSimulation::GridSimulation()
    : m_gridSize(5), m_gridDistance(100.0), m_phyMode("DsssRate1Mbps"),
      m_packetSize(1000), m_numPackets(1),
      m_sourceNode(24), m_sinkNode(0),
      m_verbose(false), m_pcapTracing(false), m_asciiTracing(false),
      m_rssiCutoff(-80) {}

void GridSimulation::Setup(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.AddValue("gridSize", "Number of nodes in one dimension of the grid", m_gridSize);
  cmd.AddValue("gridDistance", "Distance between nodes in meters", m_gridDistance);
  cmd.AddValue("phyMode", "Wifi Phy mode (e.g., DsssRate1Mbps)", m_phyMode);
  cmd.AddValue("packetSize", "Size of application packet sent", m_packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", m_numPackets);
  cmd.AddValue("sourceNode", "Source node index", m_sourceNode);
  cmd.AddValue("sinkNode", "Sink node index", m_sinkNode);
  cmd.AddValue("verbose", "Enable verbose output", m_verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", m_pcapTracing);
  cmd.AddValue("ascii", "Enable ASCII tracing", m_asciiTracing);
  cmd.AddValue("rssiCutoff", "RSSI cutoff for transmission", m_rssiCutoff);
  cmd.Parse(argc, argv);

  if (m_verbose) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  CreateNodes();
  CreateDevices();
}

void GridSimulation::CreateNodes() {
  m_nodes.Create(m_gridSize * m_gridSize);

  // Mobility
  Ptr<GridPositionAllocator> gridAlloc = CreateObject<GridPositionAllocator>();
  gridAlloc->SetMinX(0.0);
  gridAlloc->SetMinY(0.0);
  gridAlloc->SetDeltaX(m_gridDistance);
  gridAlloc->SetDeltaY(m_gridDistance);
  gridAlloc->SetGridWidth(m_gridSize);
  gridAlloc->SetLayoutType(GridPositionAllocator::ROW_MAJOR);

  MobilityModelHelper mobility;
  mobility.SetPositionAllocator(gridAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);
}

void GridSimulation::CreateDevices() {
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid("adhoc-network");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

  YansWifiPhyHelper phy;
  phy.SetErrorRateModel("ns3::NistErrorRateModel");
  phy.SetChannelNumber(1);
  phy.SetPreambleDetectionModel("ns3::ThresholdPreambleDetectionModel");

  // Distance-based RSSI cutoff
  phy.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(m_gridDistance * 2));
  phy.Set("TxPowerStart", DoubleValue(16)); // dBm
  phy.Set("TxPowerEnd", DoubleValue(16));
  phy.Set("RxGain", DoubleValue(0));
  phy.Set("RtsCtsThreshold", UintegerValue(3000));

  Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue(m_phyMode));
  Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue(m_phyMode));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  m_devices = wifi.Install(phy, mac, m_nodes);

  // Internet stack
  InternetStackHelper internet;
  OlsrHelper olsr;
  internet.SetRoutingHelper(olsr);
  internet.Install(m_nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.0.0");
  m_interfaces = address.Assign(m_devices);
}

void GridSimulation::InstallApplications() {
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(m_nodes.Get(m_sinkNode));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(m_interfaces.GetAddress(m_sinkNode, 0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(m_numPackets));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(m_packetSize));

  ApplicationContainer clientApps = echoClient.Install(m_nodes.Get(m_sourceNode));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));
}

void GridSimulation::Run() {
  InstallApplications();

  if (m_pcapTracing) {
    AsciiTraceHelper ascii;
    phy.EnablePcapAll("wireless-grid-sim");
  }

  if (m_asciiTracing) {
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wireless-grid-sim.tr"));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  GridSimulation sim;
  sim.Setup(argc, argv);
  sim.Run();
  return 0;
}