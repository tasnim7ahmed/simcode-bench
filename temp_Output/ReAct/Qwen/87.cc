#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiPerformanceExperiment");

class WiFiExperiment : public Object
{
public:
  WiFiExperiment();
  virtual ~WiFiExperiment();

  void Setup(Ptr<Node> node1, Ptr<Node> node2, double distance);
  void Start(Ptr<WifiNetDevice> txDev, Ptr<WifiNetDevice> rxDev, uint16_t port, Time duration);

private:
  void OnTx(Ptr<const Packet> packet, Mac48Address to);
  void OnRx(Ptr<const Packet> packet, Mac48Address from);
  void ReportStats();

  uint64_t m_txCount;
  uint64_t m_rxCount;
  std::vector<Time> m_delays;
  DataCollector m_dataCollector;
};

WiFiExperiment::WiFiExperiment()
  : m_txCount(0), m_rxCount(0)
{
}

WiFiExperiment::~WiFiExperiment()
{
}

void WiFiExperiment::Setup(Ptr<Node> node1, Ptr<Node> node2, double distance)
{
  // Mobility configuration
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(distance, 0.0, 0.0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(node1);
  mobility.Install(node2);

  // Wi-Fi configuration
  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211a);
  wifiHelper.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());

  NetDeviceContainer devices;
  devices.Add(wifiHelper.Install(phyHelper, wifiMac, node1));
  devices.Add(wifiHelper.Install(phyHelper, wifiMac, node2));

  InternetStackHelper stack;
  stack.Install(NodeContainer(node1, node2));

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State", StringValue("ns3::WifiPhyStateHelper"));
}

void WiFiExperiment::Start(Ptr<WifiNetDevice> txDev, Ptr<WifiNetDevice> rxDev, uint16_t port, Time duration)
{
  NS_LOG_INFO("Starting experiment...");

  // Install application
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(rxDev->GetNode());
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(duration);

  UdpClientHelper client(Ipv4Address("10.0.0.2"), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(txDev->GetNode());
  clientApp.Start(Seconds(0.0));
  clientApp.Stop(duration);

  // Set up tracing
  txDev->GetMac()->TraceConnectWithoutContext("MacTx", MakeCallback(&WiFiExperiment::OnTx, this));
  rxDev->GetMac()->TraceConnectWithoutContext("MacRx", MakeCallback(&WiFiExperiment::OnRx, this));

  Simulator::Schedule(duration, &WiFiExperiment::ReportStats, this);
}

void WiFiExperiment::OnTx(Ptr<const Packet> packet, Mac48Address to)
{
  m_txCount++;
  m_dataCollector.DescribeRun("wifi-experiment", "tx", "run");
  m_dataCollector.AddData("Transmitted Packets", 1);
}

void WiFiExperiment::OnRx(Ptr<const Packet> packet, Mac48Address from)
{
  m_rxCount++;
  m_dataCollector.DescribeRun("wifi-experiment", "rx", "run");
  m_dataCollector.AddData("Received Packets", 1);
}

void WiFiExperiment::ReportStats()
{
  std::cout << "Transmitted packets: " << m_txCount << std::endl;
  std::cout << "Received packets: " << m_rxCount << std::endl;

  if (m_txCount > 0)
  {
    double lossRate = static_cast<double>(m_txCount - m_rxCount) / m_txCount;
    std::cout << "Packet Loss Rate: " << lossRate * 100 << "%" << std::endl;
  }

  std::ostringstream oss;
  oss << "data-" << Now().GetSeconds() << ".omnet";
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(oss.str());
  m_dataCollector.Write(stream);
  stream->GetStream()->flush();

  oss.str("");
  oss << "data-" << Now().GetSeconds() << ".sqlite";
  SqliteDataOutput sqliteOutput;
  sqliteOutput.Output(m_dataCollector, oss.str());
}

int main(int argc, char *argv[])
{
  std::string format = "both";
  double distance = 100.0;
  uint16_t port = 9;
  double simulationTime = 10.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("format", "Output format (omnet, sqlite, both)", format);
  cmd.AddValue("port", "Port number for UDP traffic", port);
  cmd.AddValue("time", "Simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WiFiExperiment experiment;
  experiment.Setup(nodes.Get(0), nodes.Get(1), distance);

  Ptr<WifiNetDevice> txDev = DynamicCast<WifiNetDevice>(nodes.Get(0)->GetDevice(0));
  Ptr<WifiNetDevice> rxDev = DynamicCast<WifiNetDevice>(nodes.Get(1)->GetDevice(0));

  experiment.Start(txDev, rxDev, port, Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}