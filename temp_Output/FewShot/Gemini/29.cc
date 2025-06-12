#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultirateExperiment");

class WifiMultirateExperiment {
public:
    WifiMultirateExperiment();
    ~WifiMultirateExperiment();
    void Run(int argc, char *argv[]);

private:
    void CreateNodes();
    void InstallWifi();
    void InstallMobility();
    void InstallInternetStack();
    void InstallApplications();
    void SetupFlowMonitor();
    void CalculateThroughput();
    void CreateGnuplot();

    uint32_t m_numNodes;
    double m_simulationTime;
    std::string m_rate;
    std::string m_phyMode;
    NodeContainer m_nodes;
    WifiHelper m_wifiHelper;
    WifiMacHelper m_macHelper;
    YansWifiChannelHelper m_channelHelper;
    YansWifiPhyHelper m_phyHelper;
    MobilityHelper m_mobilityHelper;
    OlsrHelper m_olsrHelper;
    Ipv4InterfaceContainer m_interfaces;
    ApplicationContainer m_apps;
    FlowMonitorHelper m_flowMonitorHelper;
    FlowMonitor m_flowMonitor;
    std::map<FlowId, double> m_flowThroughput;
    std::string m_gnuplotFilename;
    std::string m_runName;
    double m_txPower;
};

WifiMultirateExperiment::WifiMultirateExperiment() :
    m_numNodes(100),
    m_simulationTime(10.0),
    m_rate("OfdmRate6Mbps"),
    m_phyMode("DsssRate1Mbps"),
    m_gnuplotFilename("wifi-multirate.plt"),
    m_runName("wifi-multirate"),
    m_txPower(16.0)
{
}

WifiMultirateExperiment::~WifiMultirateExperiment() {}

void WifiMultirateExperiment::Run(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes", m_numNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", m_simulationTime);
    cmd.AddValue("rate", "Wifi data rate", m_rate);
    cmd.AddValue("phyMode", "Wifi PHY mode", m_phyMode);
    cmd.AddValue("gnuplotFilename", "Gnuplot filename", m_gnuplotFilename);
    cmd.AddValue("runName", "Run name", m_runName);
    cmd.AddValue("txPower", "Transmit Power", m_txPower);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
    CreateNodes();
    InstallWifi();
    InstallMobility();
    InstallInternetStack();
    InstallApplications();
    SetupFlowMonitor();

    Simulator::Stop(Seconds(m_simulationTime));
    Simulator::Run();

    CalculateThroughput();
    CreateGnuplot();

    Simulator::Destroy();
}

void WifiMultirateExperiment::CreateNodes() {
    m_nodes.Create(m_numNodes);
}

void WifiMultirateExperiment::InstallWifi() {
    m_wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211b);

    m_channelHelper = YansWifiChannelHelper::Default();
    m_phyHelper = YansWifiPhyHelper::Default();
    m_phyHelper.SetChannel(m_channelHelper.Create());
    m_phyHelper.Set("TxPowerStart", DoubleValue(m_txPower));
    m_phyHelper.Set("TxPowerEnd", DoubleValue(m_txPower));

    m_macHelper.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = m_wifiHelper.Install(m_phyHelper, m_macHelper, m_nodes);

    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/ChannelNumber", UintegerValue(1));
    Config::Set("/NodeList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(10));

    if (!m_rate.empty()) {
        Config::Set("/NodeList/*/$ns3::WifiNetDevice/RemoteStationManager/NonUnicastMode", StringValue(m_rate));
        Config::Set("/NodeList/*/$ns3::WifiNetDevice/RemoteStationManager/UnicastMode", StringValue(m_rate));
    }
}

void WifiMultirateExperiment::InstallMobility() {
    m_mobilityHelper.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                           "X", StringValue("50.0"),
                                           "Y", StringValue("50.0"),
                                           "Z", StringValue("0.0"),
                                           "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    m_mobilityHelper.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                        "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    m_mobilityHelper.Install(m_nodes);
}

void WifiMultirateExperiment::InstallInternetStack() {
    m_olsrHelper.Install(m_nodes);
    InternetStackHelper internet;
    internet.Install(m_nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    m_interfaces = ipv4.Assign(m_nodes);
}

void WifiMultirateExperiment::InstallApplications() {
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(m_nodes.Get(m_numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(m_simulationTime - 1.0));

    UdpEchoClientHelper client(m_interfaces.GetAddress(m_numNodes - 1), port);
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    for (uint32_t i = 0; i < m_numNodes - 1; ++i) {
        ApplicationContainer clientApps = client.Install(m_nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(m_simulationTime - 2.0));
        m_apps.Add(clientApps);
    }
}

void WifiMultirateExperiment::SetupFlowMonitor() {
    m_flowMonitorHelper.SetMonitorAttribute("MaxPerHopDelay", TimeValue(Seconds(10)));
    m_flowMonitorHelper.SetMonitorAttribute("Jitter", TimeValue(Seconds(10)));
    m_flowMonitor = m_flowMonitorHelper.InstallAll();
}

void WifiMultirateExperiment::CalculateThroughput() {
    m_flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(m_flowMonitorHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000.0;
        m_flowThroughput[i->first] = throughput;
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Throughput: " << throughput << " Mbps" << std::endl;
    }
}

void WifiMultirateExperiment::CreateGnuplot() {
    Gnuplot plot(m_gnuplotFilename);
    plot.SetTitle("Wifi Multirate Experiment");
    plot.SetLegend("Flow ID", "Throughput (Mbps)");

    Gnuplot2dDataset dataset;
    dataset.SetTitle(m_runName);
    for (auto const& [flowId, throughput] : m_flowThroughput) {
        dataset.Add(flowId, throughput);
    }
    plot.AddDataset(dataset);

    std::ofstream outfile(m_gnuplotFilename.c_str());
    plot.GenerateOutput(outfile);
    outfile.close();

    std::cout << "Gnuplot file generated: " << m_gnuplotFilename << std::endl;
}

int main(int argc, char *argv[]) {
    LogComponentEnable("WifiMultirateExperiment", LOG_LEVEL_INFO);
    WifiMultirateExperiment experiment;
    experiment.Run(argc, argv);
    return 0;
}