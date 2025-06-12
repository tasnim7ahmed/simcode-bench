#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-phy-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-helper.h"
#include <vector>
#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultirateAdhocExperiment");

class WifiMultirateExperiment
{
public:
    WifiMultirateExperiment();
    void Run(int argc, char *argv[]);
private:
    void SetupMobility();
    void SetupWiFi();
    void SetupRouting();
    void InstallApplications();
    void MonitorThroughput();
    void ScheduleThroughput();
    void CollectGnuplotResults();
    void ConfigureLoggers(std::string logLevel);

    uint32_t m_nNodes;
    uint32_t m_nFlows;
    double m_simTime;
    double m_txp; // Transmission power
    uint32_t m_nodeSpeed;
    uint32_t m_flowPacketSize;
    DataRate m_flowDataRate;
    std::string m_phyMode;
    std::string m_logLevel;

    NodeContainer m_nodes;
    NetDeviceContainer m_devices;
    Ipv4InterfaceContainer m_interfaces;
    std::vector<ApplicationContainer> m_sinkApps;
    std::vector<ApplicationContainer> m_sourceApps;
    Gnuplot m_gnuplot;
    std::vector<double> m_timeStamps;
    std::vector<double> m_throughputSamples;
    Ptr<FlowMonitor> m_flowMonitor;
    FlowMonitorHelper m_flowHelper;
};

WifiMultirateExperiment::WifiMultirateExperiment()
  : m_nNodes(100),
    m_nFlows(20),
    m_simTime(60.0),
    m_txp(16.0),
    m_nodeSpeed(5),
    m_flowPacketSize(1024),
    m_flowDataRate("1Mbps"),
    m_phyMode("HtMcs7"),
    m_logLevel("INFO"),
    m_gnuplot("wifi-multirate-throughput.png")
{
}

void WifiMultirateExperiment::ConfigureLoggers(std::string logLevel)
{
    if (logLevel == "DEBUG") {
        LogComponentEnable("WifiMultirateAdhocExperiment", LOG_LEVEL_DEBUG);
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    } else if (logLevel == "INFO") {
        LogComponentEnable("WifiMultirateAdhocExperiment", LOG_LEVEL_INFO);
    }
}

void WifiMultirateExperiment::SetupMobility()
{
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> posVar = CreateObject<UniformRandomVariable>();
    posVar->SetAttribute("Min", DoubleValue(0));
    posVar->SetAttribute("Max", DoubleValue(500));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", PointerValue(posVar),
                                 "Y", PointerValue(posVar));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator"));
    mobility.Install(m_nodes);
}

void WifiMultirateExperiment::SetupWiFi()
{
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.SetErrorRateModel("ns3::NistErrorRateModel");

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n);

    wifiHelper.SetRemoteStationManager("ns3::MinstrelWifiManager");

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    m_devices = wifiHelper.Install(wifiPhy, wifiMac, m_nodes);
}

void WifiMultirateExperiment::SetupRouting()
{
    InternetStackHelper internet;
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(olsr, 10);
    listRouting.Add(staticRouting, 1);
    internet.SetRoutingHelper(listRouting);
    internet.Install(m_nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    m_interfaces = address.Assign(m_devices);
}

void WifiMultirateExperiment::InstallApplications()
{
    // We pair up sender/receiver randomly for nFlows
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    for (uint32_t i = 0; i < m_nFlows; ++i)
    {
        uint32_t src, dst;
        do {
            src = rand->GetInteger(0, m_nNodes - 1);
            dst = rand->GetInteger(0, m_nNodes - 1);
        } while (src == dst);

        uint16_t port = 10000 + i;
        UdpServerHelper server(port);
        ApplicationContainer sinkApp = server.Install(m_nodes.Get(dst));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(m_simTime));
        m_sinkApps.push_back(sinkApp);

        UdpClientHelper client(m_interfaces.GetAddress(dst), port);
        client.SetAttribute("MaxPackets", UintegerValue(0));
        client.SetAttribute("Interval", TimeValue(Seconds(double(m_flowPacketSize * 8) / m_flowDataRate.GetBitRate())));
        client.SetAttribute("PacketSize", UintegerValue(m_flowPacketSize));
        ApplicationContainer clientApp = client.Install(m_nodes.Get(src));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(m_simTime));
        m_sourceApps.push_back(clientApp);
    }
}

void WifiMultirateExperiment::MonitorThroughput()
{
    FlowMonitor::FlowStatsContainer stats = m_flowMonitor->GetFlowStats();
    double throughput = 0.0;
    for (auto& flow : stats)
    {
        throughput += (flow.second.rxBytes * 8.0) / (m_simTime * 1000000.0); // Mbps
    }
    double now = Simulator::Now().GetSeconds();
    m_timeStamps.push_back(now);
    m_throughputSamples.push_back(throughput);
    NS_LOG_INFO("Time " << now << "s: Aggregate throughput: " << throughput << " Mbps");

    // Schedule next measurement if simulation is running
    if (now + 1.0 < m_simTime) {
        Simulator::Schedule(Seconds(1.0), &WifiMultirateExperiment::MonitorThroughput, this);
    }
}

void WifiMultirateExperiment::ScheduleThroughput()
{
    Simulator::Schedule(Seconds(1.0), &WifiMultirateExperiment::MonitorThroughput, this);
}

void WifiMultirateExperiment::CollectGnuplotResults()
{
    Gnuplot2dDataset dataset("Aggregate Throughput");
    for (size_t i = 0; i < m_timeStamps.size(); ++i)
    {
        dataset.Add(m_timeStamps[i], m_throughputSamples[i]);
    }
    m_gnuplot.AddDataset(dataset);

    std::ofstream plotFile("wifi-multirate-throughput.plt");
    m_gnuplot.SetTitle("WiFi Adhoc Multirate Aggregate Throughput");
    m_gnuplot.SetLegend("Time (s)", "Throughput (Mbps)");
    m_gnuplot.GenerateOutput(plotFile);
    plotFile.close();
    NS_LOG_INFO("Throughput results written to wifi-multirate-throughput.png and wifi-multirate-throughput.plt");
}

void WifiMultirateExperiment::Run(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", m_nNodes);
    cmd.AddValue("nFlows", "Number of flows", m_nFlows);
    cmd.AddValue("simTime", "Simulation time", m_simTime);
    cmd.AddValue("phyMode", "WiFi PHY mode (e.g., HtMcs7)", m_phyMode);
    cmd.AddValue("dataRate", "UDP application data rate", m_flowDataRate);
    cmd.AddValue("packetSize", "UDP application packet size", m_flowPacketSize);
    cmd.AddValue("logLevel", "Logging verbosity (INFO or DEBUG)", m_logLevel);
    cmd.Parse(argc, argv);

    ConfigureLoggers(m_logLevel);

    NS_LOG_INFO("Creating " << m_nNodes << " nodes.");
    m_nodes.Create(m_nNodes);

    SetupMobility();
    SetupWiFi();
    SetupRouting();
    InstallApplications();

    m_flowMonitor = m_flowHelper.InstallAll();

    ScheduleThroughput();

    NS_LOG_INFO("Running simulation...");
    Simulator::Stop(Seconds(m_simTime + 2));
    Simulator::Run();

    m_flowMonitor->SerializeToXmlFile("wifi-multirate-flowmon.xml", true, true);

    CollectGnuplotResults();

    NS_LOG_INFO("Simulation complete");
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    WifiMultirateExperiment experiment;
    experiment.Run(argc, argv);

    std::cout << "=== Build Instructions ===\n";
    std::cout << "$ ./waf build\n";
    std::cout << "=== Run Instructions ===\n";
    std::cout << "$ ./waf --run \"wifi-multirate-experiment [optional args]\"\n";
    std::cout << "Example: ./waf --run \"wifi-multirate-experiment --nNodes=100 --nFlows=20 --simTime=60\"\n";
    std::cout << "=== Debugging ===\n";
    std::cout << "$ NS_LOG=WifiMultirateAdhocExperiment=level_debug ./waf --run \"wifi-multirate-experiment\"\n";
    return 0;
}