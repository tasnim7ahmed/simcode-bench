#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

class ManetSimulation {
public:
  ManetSimulation();
  void Run(int argc, char *argv[]);
private:
  std::string m_protocolName;
  uint32_t m_nNodes;
  double m_totalTime;
  double m_txp;
  double m_areaX;
  double m_areaY;
  bool m_flowMonitor;
  bool m_mobilityTrace;

  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;

  void SetupMobility();
  void SetupWifi();
  void SetupRoutingProtocol();
  void SetupApplications();
  void SetupFlowMonitor();
  void GenerateOutput();
};

ManetSimulation::ManetSimulation()
  : m_protocolName("AODV"),
    m_nNodes(50),
    m_totalTime(200.0),
    m_txp(7.5),
    m_areaX(300.0),
    m_areaY(1500.0),
    m_flowMonitor(false),
    m_mobilityTrace(false)
{
}

void ManetSimulation::Run(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.AddValue("protocol", "Routing protocol to use (DSDV, AODV, OLSR, DSR)", m_protocolName);
  cmd.AddValue("nodes", "Number of nodes", m_nNodes);
  cmd.AddValue("time", "Simulation time, s", m_totalTime);
  cmd.AddValue("txp", "Transmission power level (dBm)", m_txp);
  cmd.AddValue("areaX", "Width of the area (meters)", m_areaX);
  cmd.AddValue("areaY", "Height of the area (meters)", m_areaY);
  cmd.AddValue("flowMonitor", "Enable Flow Monitor", m_flowMonitor);
  cmd.AddValue("mobilityTrace", "Enable Mobility Tracing", m_mobilityTrace);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::Ipv4GlobalRouting::PerfomanceMode", BooleanValue(true));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(1000));

  nodes.Create(m_nNodes);

  SetupMobility();
  SetupWifi();
  SetupRoutingProtocol();

  InternetStackHelper internet;
  if (m_protocolName == "DSR") {
    internet.SetRoutingHelper(dsr::DsrRoutingHelper()); // DSR uses its own helper
  } else {
    internet.Install(nodes);
  }

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  interfaces = address.Assign(devices);

  SetupApplications();

  Simulator::Stop(Seconds(m_totalTime));
  Simulator::Run();

  GenerateOutput();

  if (m_flowMonitor) {
    SetupFlowMonitor();
  }

  Simulator::Destroy();
}

void ManetSimulation::SetupMobility() {
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaX) + "]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(m_areaY) + "]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  mobility.Install(nodes);

  if (m_mobilityTrace) {
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("manet-mobility-trace.mob");
    mobility.EnableAsciiAll(stream);
  }
}

void ManetSimulation::SetupWifi() {
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set("TxPowerStart", DoubleValue(m_txp));
  wifiPhy.Set("TxPowerEnd", DoubleValue(m_txp));
  wifiPhy.Set("RxGain", DoubleValue(0));
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));
  devices = wifi.Install(wifiPhy, wifiMac, nodes);
}

void ManetSimulation::SetupRoutingProtocol() {
  if (m_protocolName == "DSDV") {
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsdv);
    stack.Install(nodes);
  } else if (m_protocolName == "AODV") {
    aodv::AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);
  } else if (m_protocolName == "OLSR") {
    OlsrHelper olsr;
    InternetStackHelper stack;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);
  } else if (m_protocolName == "DSR") {
    dsr::DsrHelper dsr;
    dsr::DsrMainHelper dsrMain(dsr);
    dsrMain.Install(dsr, nodes);
  }
}

void ManetSimulation::SetupApplications() {
  uint16_t port = 9;
  ApplicationContainer apps;
  for (uint32_t i = 0; i < 10; ++i) {
    uint32_t src = i * 2;
    uint32_t dst = src + 1;
    while (dst == src || dst >= m_nNodes) {
      dst = rand() % m_nNodes;
    }

    InetSocketAddress sinkAddr(InetSocketAddress(interfaces.GetAddress(dst), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddr);
    apps.Add(packetSinkHelper.Install(nodes.Get(src)));
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddr);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1024bps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    double startTime = 50.0 + (static_cast<double>(rand()) / RAND_MAX);
    apps.Add(onoff.Install(nodes.Get(src)));
    apps.Start(Seconds(startTime));
    apps.Stop(Seconds(m_totalTime - 1));
  }
}

void ManetSimulation::GenerateOutput() {
  std::ofstream outFile;
  std::string fileName = "manet-performance-" + m_protocolName + ".csv";
  outFile.open(fileName.c_str(), std::ios::out | std::ios::app);
  if (!outFile.is_open()) {
    NS_LOG_ERROR("Could not open output file.");
    return;
  }

  outFile << m_nNodes << "," << m_totalTime << "," << m_txp << "," << m_areaX << "," << m_areaY << ",";
  uint32_t received = 0;
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(nodes.Get(i)->GetApplication(0));
    if (sink) {
      received += sink->GetTotalRx();
    }
  }
  outFile << received << std::endl;
  outFile.close();
}

void ManetSimulation::SetupFlowMonitor() {
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Simulator::Stop(Seconds(m_totalTime));
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  std::ofstream out("manet-flow-stats-" + m_protocolName + ".txt");
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    out << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    out << "  Tx Packets: " << i->second.txPackets << "\n";
    out << "  Rx Packets: " << i->second.rxPackets << "\n";
    out << "  Lost Packets: " << i->second.lostPackets << "\n";
    out << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps\n";
  }
  out.close();
}

int main(int argc, char *argv[]) {
  ManetSimulation sim;
  sim.Run(argc, argv);
  return 0;
}