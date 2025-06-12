#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/olsr-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

class ManetRoutingComparison : public Object
{
public:
  static TypeId GetTypeId(void);
  ManetRoutingComparison();
  void Run();

private:
  void SetupMobility(Ptr<NodeContainer> nodes, double areaWidth, double areaHeight, double maxSpeed, double pauseTime);
  void SetupWifi(Ptr<NodeContainer> nodes, double txPowerLevel, YansWifiPhyHelper& wifiPhy, WifiMacHelper& wifiMac, WifiHelper& wifi);
  void SetupApplications(NodeContainer nodes, uint16_t port, uint32_t payloadSize, uint32_t numPairs);
  void SetupTracing(std::string filename, bool enableMobilityTrace, bool enableFlowMonitor);

  std::string m_protocolName;
  uint32_t m_numNodes;
  double m_areaWidth;
  double m_areaHeight;
  double m_maxSpeed;
  double m_pauseTime;
  double m_txPowerLevel;
  uint32_t m_numUdpPairs;
  uint16_t m_udpPort;
  uint32_t m_payloadSize;
  Time m_simulationTime;
  bool m_enableMobilityTrace;
  bool m_enableFlowMonitor;
  std::string m_outputFilename;
};

TypeId
ManetRoutingComparison::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::ManetRoutingComparison")
    .SetParent<Object>()
    .AddConstructor<ManetRoutingComparison>()
    .AddAttribute("Protocol", "Routing protocol to use (DSDV, AODV, OLSR, DSR)",
                  StringValue("AODV"),
                  MakeStringAccessor(&ManetRoutingComparison::m_protocolName),
                  MakeStringChecker())
    .AddAttribute("NumNodes", "Number of mobile nodes",
                  UintegerValue(50),
                  MakeUintegerAccessor(&ManetRoutingComparison::m_numNodes),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("AreaWidth", "Width of simulation area in meters",
                  DoubleValue(300.0),
                  MakeDoubleAccessor(&ManetRoutingComparison::m_areaWidth),
                  MakeDoubleChecker<double>())
    .AddAttribute("AreaHeight", "Height of simulation area in meters",
                  DoubleValue(1500.0),
                  MakeDoubleAccessor(&ManetRoutingComparison::m_areaHeight),
                  MakeDoubleChecker<double>())
    .AddAttribute("MaxSpeed", "Maximum speed of nodes in m/s",
                  DoubleValue(20.0),
                  MakeDoubleAccessor(&ManetRoutingComparison::m_maxSpeed),
                  MakeDoubleChecker<double>())
    .AddAttribute("PauseTime", "Node pause time in seconds",
                  DoubleValue(0.0),
                  MakeDoubleAccessor(&ManetRoutingComparison::m_pauseTime),
                  MakeDoubleChecker<double>())
    .AddAttribute("TxPowerLevel", "Transmission power level in dBm",
                  DoubleValue(7.5),
                  MakeDoubleAccessor(&ManetRoutingComparison::m_txPowerLevel),
                  MakeDoubleChecker<double>())
    .AddAttribute("NumUdpPairs", "Number of UDP source/sink pairs",
                  UintegerValue(10),
                  MakeUintegerAccessor(&ManetRoutingComparison::m_numUdpPairs),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("UdpPort", "UDP port for applications",
                  UintegerValue(9),
                  MakeUintegerAccessor(&ManetRoutingComparison::m_udpPort),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute("PayloadSize", "Size of UDP packet payload in bytes",
                  UintegerValue(512),
                  MakeUintegerAccessor(&ManetRoutingComparison::m_payloadSize),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("SimulationTime", "Total simulation time in seconds",
                  TimeValue(Seconds(200)),
                  MakeTimeAccessor(&ManetRoutingComparison::m_simulationTime),
                  MakeTimeChecker())
    .AddAttribute("EnableMobilityTrace", "Enable mobility tracing",
                  BooleanValue(false),
                  MakeBooleanAccessor(&ManetRoutingComparison::m_enableMobilityTrace),
                  MakeBooleanChecker())
    .AddAttribute("EnableFlowMonitor", "Enable flow monitoring",
                  BooleanValue(true),
                  MakeBooleanAccessor(&ManetRoutingComparison::m_enableFlowMonitor),
                  MakeBooleanChecker())
    .AddAttribute("OutputFilename", "Output CSV filename",
                  StringValue("manet-routing-comparison.csv"),
                  MakeStringAccessor(&ManetRoutingComparison::m_outputFilename),
                  MakeStringChecker());
  return tid;
}

ManetRoutingComparison::ManetRoutingComparison()
{
  NS_LOG_FUNCTION(this);
}

void
ManetRoutingComparison::Run()
{
  NS_LOG_FUNCTION(this);

  NodeContainer nodes;
  nodes.Create(m_numNodes);

  YansWifiPhyHelper wifiPhy;
  WifiMacHelper wifiMac;
  WifiHelper wifi;

  SetupWifi(&nodes, m_txPowerLevel, wifiPhy, wifiMac, wifi);

  InternetStackHelper internet;
  Ipv4ListRoutingHelper listRH;

  if (m_protocolName == "DSDV")
  {
    DsdvHelper dsdv;
    listRH.Add(dsdv, 10);
  }
  else if (m_protocolName == "AODV")
  {
    AodvHelper aodv;
    listRH.Add(aodv, 10);
  }
  else if (m_protocolName == "OLSR")
  {
    OlsrHelper olsr;
    listRH.Add(olsr, 10);
  }
  else if (m_protocolName == "DSR")
  {
    DsrHelper dsr;
    internet.SetRoutingHelper(dsr);
  }

  internet.SetRoutingHelper(listRH);
  internet.Install(nodes);

  SetupMobility(&nodes, m_areaWidth, m_areaHeight, m_maxSpeed, m_pauseTime);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(wifiPhy.GetPhyDevices());

  SetupApplications(nodes, m_udpPort, m_payloadSize, m_numUdpPairs);

  SetupTracing(m_outputFilename, m_enableMobilityTrace, m_enableFlowMonitor);

  Simulator::Stop(m_simulationTime);
  Simulator::Run();
  Simulator::Destroy();
}

void
ManetRoutingComparison::SetupMobility(Ptr<NodeContainer> nodes, double areaWidth, double areaHeight,
                                     double maxSpeed, double pauseTime)
{
  NS_LOG_FUNCTION(this << nodes << areaWidth << areaHeight << maxSpeed << pauseTime);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", CreateObjectWithAttributes<UniformRandomVariable>("Min", DoubleValue(0.0), "Max", DoubleValue(areaWidth)),
                                "Y", CreateObjectWithAttributes<UniformRandomVariable>("Min", DoubleValue(0.0), "Max", DoubleValue(areaHeight)));

  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", CreateObjectWithAttributes<ConstantRandomVariable>("Constant", DoubleValue(maxSpeed)),
                            "Pause", CreateObjectWithAttributes<ConstantRandomVariable>("Constant", DoubleValue(pauseTime)),
                            "PositionAllocator", CreateObject<RandomRectanglePositionAllocator>());
  mobility.Install(*nodes);
}

void
ManetRoutingComparison::SetupWifi(Ptr<NodeContainer> nodes, double txPowerLevel,
                                  YansWifiPhyHelper& wifiPhy, WifiMacHelper& wifiMac, WifiHelper& wifi)
{
  NS_LOG_FUNCTION(this << nodes << txPowerLevel);

  wifiPhy.Set("TxPowerStart", DoubleValue(txPowerLevel));
  wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerLevel));
  wifiPhy.Set("TxGain", DoubleValue(0));
  wifiPhy.Set("RxGain", DoubleValue(0));
  wifiPhy.Set("ChannelNumber", UintegerValue(1));

  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, *nodes);
}

void
ManetRoutingComparison::SetupApplications(NodeContainer nodes, uint16_t port, uint32_t payloadSize, uint32_t numPairs)
{
  NS_LOG_FUNCTION(this << &nodes << port << payloadSize << numPairs);

  for (uint32_t i = 0; i < numPairs; ++i)
  {
    uint32_t src = rand() % nodes.GetN();
    uint32_t dst = rand() % nodes.GetN();
    while (dst == src)
      dst = rand() % nodes.GetN();

    Ptr<Node> sender = nodes.Get(src);
    Ptr<Node> receiver = nodes.Get(dst);

    InetSocketAddress sinkAddr(Ipv4Address::GetAny(), port);
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = packetSinkHelper.Install(receiver);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(m_simulationTime);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(sinkAddr));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000000bps")));

    ApplicationContainer app = onoff.Install(sender);
    double startTime = 50.0 + (static_cast<double>(rand()) / RAND_MAX);
    app.Start(Seconds(startTime));
    app.Stop(m_simulationTime);
  }
}

void
ManetRoutingComparison::SetupTracing(std::string filename, bool enableMobilityTrace, bool enableFlowMonitor)
{
  NS_LOG_FUNCTION(this << filename << enableMobilityTrace << enableFlowMonitor);

  AsciiTraceHelper ascii;
  if (enableMobilityTrace)
  {
    MobilityHelper::EnableAsciiAll(ascii.CreateFileStream("manet-mobility-trace.mob"));
  }

  if (enableFlowMonitor)
  {
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Schedule(m_simulationTime - Seconds(0.001), &FlowMonitor::CheckForLostPackets, monitor);
    monitor->SerializeToXmlFile(filename, false, false);
  }
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("ManetRoutingComparison", LOG_LEVEL_INFO);

  ManetRoutingComparison experiment;
  CommandLine cmd(__FILE__);
  cmd.AddValue("Protocol", "Routing protocol to use (DSDV, AODV, OLSR, DSR)");
  cmd.AddValue("NumNodes", "Number of mobile nodes");
  cmd.AddValue("AreaWidth", "Width of simulation area in meters");
  cmd.AddValue("AreaHeight", "Height of simulation area in meters");
  cmd.AddValue("MaxSpeed", "Maximum speed of nodes in m/s");
  cmd.AddValue("PauseTime", "Node pause time in seconds");
  cmd.AddValue("TxPowerLevel", "Transmission power level in dBm");
  cmd.AddValue("NumUdpPairs", "Number of UDP source/sink pairs");
  cmd.AddValue("UdpPort", "UDP port for applications");
  cmd.AddValue("PayloadSize", "Size of UDP packet payload in bytes");
  cmd.AddValue("SimulationTime", "Total simulation time in seconds");
  cmd.AddValue("EnableMobilityTrace", "Enable mobility tracing");
  cmd.AddValue("EnableFlowMonitor", "Enable flow monitoring");
  cmd.AddValue("OutputFilename", "Output CSV filename");

  cmd.Parse(argc, argv);

  experiment.Run();

  return 0;
}