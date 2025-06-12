#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/gnuplot.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-server.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExperiment");

class Experiment {
public:
  Experiment() : m_simulationTime(10), m_txRate("6Mbps"), m_managerType("AarfWifiManager") {}

  void SetSimulationTime(double time) { m_simulationTime = time; }
  void SetTxRate(std::string rate) { m_txRate = rate; }
  void SetManagerType(std::string type) { m_managerType = type; }

  Vector GetPosition(uint32_t node) const { return m_positions[node]; }

  void Run() {
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    Ssid ssid = Ssid("adhoc-network");
    mac.Set("Ssid", SsidValue(ssid));

    wifi.SetRemoteStationManager(m_managerType, "DataMode", StringValue(m_txRate));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator", "X", StringValue("50.0"),
                                  "Y", StringValue("50.0"), "Rho", StringValue("10.0"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Ptr<MobilityModel> mob0 = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob1 = nodes.Get(1)->GetObject<MobilityModel>();
    m_positions[0] = mob0->GetPosition();
    m_positions[1] = mob1->GetPosition();

    PacketSocketHelper packetSocketHelper;
    packetSocketHelper.Install(nodes.Get(1));

    TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");

    Ptr<Socket> sink = Socket::CreateSocket(nodes.Get(1), tid);
    InetSocketAddress local;
    local.SetPort(5000);
    sink->Bind(local);

    OnOffHelper onoff("ns3::PacketSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 5000)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(m_simulationTime));

    Simulator::Stop(Seconds(m_simulationTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    double throughput = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.sourceAddress == "10.1.1.1" && t.destinationAddress == "10.1.1.2") {
        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
      }
    }

    Simulator::Destroy();

    std::string filename = "adhoc-wifi-" + m_managerType + "-" + m_txRate + ".plt";
    Gnuplot2dDataset dataset;
    dataset.SetTitle(m_managerType + " - " + m_txRate);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    dataset.Add(m_simulationTime, throughput);

    Gnuplot plot("throughput.plt");
    plot.SetTitle("Adhoc WiFi Throughput");
    plot.SetLegend("Time (s)", "Throughput (Mbps)");
    plot.AddDataset(dataset);
    plot.GenerateOutput("throughput.plt");

    std::cout << "Simulation completed for " << m_managerType << " at " << m_txRate
              << ". Throughput: " << throughput << " Mbps" << std::endl;
  }

private:
  double m_simulationTime;
  std::string m_txRate;
  std::string m_managerType;
  Vector m_positions[2];
};

int main(int argc, char *argv[]) {
  LogComponentEnable("AdhocWifiExperiment", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  std::vector<std::string> rates = {"6Mbps", "9Mbps", "12Mbps"};
  std::vector<std::string> managers = {"AarfWifiManager", "AmrrWifiManager", "ArfWifiManager"};

  for (const auto &rate : rates) {
    for (const auto &manager : managers) {
      Experiment experiment;
      experiment.SetSimulationTime(10.0);
      experiment.SetTxRate(rate);
      experiment.SetManagerType(manager);
      experiment.Run();
    }
  }

  return 0;
}