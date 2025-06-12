#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

enum RoutingProtocol {
  DSDV,
  AODV,
  OLSR,
  DSR
};

class ManetSimulation {
public:
  ManetSimulation();
  void Run();

private:
  void SetupMobility(Ptr<NodeContainer> nodes);
  void InstallInternetStack(NodeContainer& nodes, RoutingProtocol protocol);
  void SetupApplications(NodeContainer& nodes, uint32_t nSinks);
  void Teardown();

  RoutingProtocol m_protocol;
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  uint32_t m_nNodes;
  double m_txPower;
  double m_areaWidth;
  double m_areaHeight;
  double m_mobilitySpeed;
  uint32_t m_udpPairs;
  bool m_flowMonitor;
  bool m_mobilityTrace;
  std::string m_csvFileName;
};

ManetSimulation::ManetSimulation()
  : m_protocol(DSDV),
    m_nNodes(50),
    m_txPower(7.5),
    m_areaWidth(300),
    m_areaHeight(1500),
    m_mobilitySpeed(20),
    m_udpPairs(10),
    m_flowMonitor(false),
    m_mobilityTrace(false),
    m_csvFileName("manet-output.csv")
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("protocol", "Routing protocol (0=DSDV, 1=AODV, 2=OLSR, 3=DSR)", m_protocol);
  cmd.AddValue("nNodes", "Number of nodes", m_nNodes);
  cmd.AddValue("txPower", "Transmission power (dBm)", m_txPower);
  cmd.AddValue("areaWidth", "Area width in meters", m_areaWidth);
  cmd.AddValue("areaHeight", "Area height in meters", m_areaHeight);
  cmd.AddValue("mobilitySpeed", "Node mobility speed (m/s)", m_mobilitySpeed);
  cmd.AddValue("udpPairs", "Number of UDP source/sink pairs", m_udpPairs);
  cmd.AddValue("flowMonitor", "Enable flow monitor", m_flowMonitor);
  cmd.AddValue("mobilityTrace", "Enable mobility tracing", m_mobilityTrace);
  cmd.AddValue("csvFile", "CSV output file name", m_csvFileName);
  cmd.Parse(Simulator::Get argc(), Simulator::Get argv());
}

void ManetSimulation::Run() {
  LogComponentEnable("ManetRoutingComparison", LOG_LEVEL_INFO);

  m_nodes.Create(m_nNodes);

  WifiMacHelper wifiMac = WifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  WifiHelper wifi = WifiHelper::Default();
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));
  m_devices = wifi.Install(wifiPhy, wifiMac, m_nodes);

  wifiPhy.EnablePcapAll("manet-wifi");

  SetupMobility(&m_nodes);

  if (m_mobilityTrace) {
    AsciiTraceHelper ascii;
    MobilityHelper::EnableAsciiAll(ascii.CreateFileStream("manet-mobility-trace.mob"));
  }

  InstallInternetStack(m_nodes, m_protocol);

  SetupApplications(m_nodes, m_udpPairs);

  if (m_flowMonitor) {
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Schedule(Seconds(200), &FlowMonitor::SerializeToXmlFile, monitor, "manet-flow.xml", false, false);
  }

  NS_LOG_INFO("Starting simulation for 200 seconds...");
  Simulator::Stop(Seconds(200));
  Simulator::Run();

  Teardown();
}

void ManetSimulation::SetupMobility(Ptr<NodeContainer> nodes) {
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaWidth) + "]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaHeight) + "]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(m_mobilitySpeed) + "]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.Install(*nodes);
}

void ManetSimulation::InstallInternetStack(NodeContainer& nodes, RoutingProtocol protocol) {
  InternetStackHelper stack;

  switch (protocol) {
    case DSDV:
      stack.SetRoutingHelper(DsdvHelper());
      break;
    case AODV:
      stack.SetRoutingHelper(AodvHelper());
      break;
    case OLSR:
      stack.SetRoutingHelper(OlsrHelper());
      break;
    case DSR:
      {
        DsrStackHelper dsr;
        stack.SetRoutingHelper(dsr);
        DsrHelper dsrApp;
        ApplicationContainer dsrApps;
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
          dsrApps.Add(dsrApp.Install(nodes.Get(i)));
        }
        dsrApps.Start(Seconds(0));
        dsrApps.Stop(Seconds(200));
      }
      break;
    default:
      stack.SetRoutingHelper(DsdvHelper());
      break;
  }

  stack.Install(nodes);
  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  m_interfaces = address.Assign(m_devices);
}

void ManetSimulation::SetupApplications(NodeContainer& nodes, uint32_t nSinks) {
  std::ofstream csvFile;
  csvFile.open(m_csvFileName, std::ios::out | std::ios::trunc);
  csvFile << "SinkNode,TotalRx\n";
  csvFile.close();

  for (uint32_t i = 0; i < nSinks; ++i) {
    uint32_t src = rand() % m_nNodes;
    uint32_t dst = rand() % m_nNodes;
    while (dst == src) dst = rand() % m_nNodes;

    InetSocketAddress sinkAddr = InetSocketAddress(m_interfaces.GetAddress(dst), 8080);
    PacketSinkHelper packetSink("ns3::UdpSocketFactory", sinkAddr);
    ApplicationContainer sinkApps = packetSink.Install(nodes.Get(dst));
    sinkApps.Start(Seconds(0));
    sinkApps.Stop(Seconds(200));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddr);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("512Kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer srcApps = onoff.Install(nodes.Get(src));
    srcApps.Start(Seconds(50 + (double)rand()/RAND_MAX));
    srcApps.Stop(Seconds(200));

    std::ostringstream oss;
    oss << "Node-" << src << "-to-" << dst << ".pcap";
    wifiPhy.EnablePcap(oss.str(), m_devices.Get(src));
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",
                  MakeCallback([](std::string path, Ptr<const Packet> p) {
                    static std::map<uint32_t, uint64_t> rxCount;
                    uint32_t nodeId = atoi(path.substr(path.find("NodeList/") + 9, path.find("/ApplicationList") - (path.find("NodeList/") + 9)).c_str());
                    rxCount[nodeId] += p->GetSize();
                    static Time lastWrite = Seconds(0);
                    if (Simulator::Now() - lastWrite > Seconds(1)) {
                      std::ofstream csvFile;
                      csvFile.open(m_csvFileName, std::ios::out | std::ios::app);
                      for (auto& pair : rxCount) {
                        csvFile << pair.first << "," << pair.second << "\n";
                      }
                      rxCount.clear();
                      lastWrite = Simulator::Now();
                    }
                  }));
}

void ManetSimulation::Teardown() {
  Simulator::Destroy();
}

int main(int argc, char* argv[]) {
  ManetSimulation sim;
  sim.Run();
  return 0;
}