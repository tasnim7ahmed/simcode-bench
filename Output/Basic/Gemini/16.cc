#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExperiment");

class Experiment {
public:
  Experiment(std::string wifiRate, std::string rtsCtsThreshold, std::string manager)
      : m_wifiRate(wifiRate),
        m_rtsCtsThreshold(rtsCtsThreshold),
        m_manager(manager),
        m_nodeA(Names::Find<Node>("nodeA")),
        m_nodeB(Names::Find<Node>("nodeB")) {}

  void Run(double simulationTime) {
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue(m_wifiRate));

    NodeContainer nodes;
    nodes.Create(2);
    m_nodeA = nodes.Get(0);
    m_nodeB = nodes.Get(1);
    Names::Add("nodeA", m_nodeA);
    Names::Add("nodeB", m_nodeB);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(i.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("1Mb/s"));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime + 1));

    PacketSinkHelper sink("ns3::UdpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
    app = sink.Install(nodes.Get(1));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    AnimationInterface anim("adhoc-wifi.xml");

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double throughput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.sourceAddress == i.GetAddress(0,0) && t.destinationAddress == i.GetAddress(1,0))
      {
        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
        NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
        NS_LOG_UNCOND("  Tx Bytes:   " << i->second.txBytes);
        NS_LOG_UNCOND("  Rx Bytes:   " << i->second.rxBytes);
        NS_LOG_UNCOND("  Throughput: " << throughput << " Mbps");
        break;
      }
    }

    Simulator::Destroy();

    WriteGnuplotData(throughput);
    PlotGnuplot(m_wifiRate);
  }

  Vector3D GetPosition(Ptr<Node> node) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    return mobility->GetPosition();
  }

private:
  void WriteGnuplotData(double throughput) {
    std::ofstream outfile(m_wifiRate + ".dat", std::ios::app);
    outfile << m_wifiRate << " " << throughput << std::endl;
    outfile.close();
  }

  void PlotGnuplot(std::string title) {
    std::string filename = title;
    std::string plotFilename = filename + ".plt";
    std::string graphicsFilename = filename + ".png";
    std::string dataFilename = filename + ".dat";

    std::ofstream plotFile(plotFilename.c_str());

    plotFile << "set terminal png size 640,480" << std::endl;
    plotFile << "set output \"" << graphicsFilename << "\"" << std::endl;
    plotFile << "set title \"" << title << "\"" << std::endl;
    plotFile << "set xlabel \"Wifi Rate\"" << std::endl;
    plotFile << "set ylabel \"Throughput (Mbps)\"" << std::endl;
    plotFile << "plot \"" << dataFilename << "\" using 1:2 with linespoints title \"" << title << "\"" << std::endl;

    plotFile.close();

    std::string command = "gnuplot " + plotFilename;
    system(command.c_str());
  }

private:
  std::string m_wifiRate;
  std::string m_rtsCtsThreshold;
  std::string m_manager;
  Ptr<Node> m_nodeA;
  Ptr<Node> m_nodeB;
};

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  double simulationTime = 10.0;

  std::vector<std::string> wifiRates = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};
  std::vector<std::string> rtsCtsThresholds = {"250", "500", "1000", "2000"};
  std::vector<std::string> managers = {"AarfWifiManager", "AmrrWifiManager"};

  for (const auto& wifiRate : wifiRates) {
    Experiment experiment(wifiRate, "250", "AarfWifiManager");
    experiment.Run(simulationTime);
  }
  return 0;
}