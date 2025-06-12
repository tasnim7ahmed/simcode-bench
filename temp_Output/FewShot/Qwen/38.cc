#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiNetworkSimulation");

class NodeStatistics {
public:
    NodeStatistics(Ptr<Node> node, uint32_t nodeId);

    void PhyStateTrace(std::string context, Time now, WifiPhyState state);
    void RxTrace(Ptr<const Packet> packet, const Address &);
    void TxTrace(Ptr<const Packet> packet);
    void PrintStats(Time interval);
    void UpdatePower(double power);
    void OutputGnuplotData(std::ofstream &outFile);

private:
    uint32_t m_nodeId;
    std::map<WifiPhyState, Time> m_stateTime;
    Time m_lastUpdateTime;
    uint64_t m_bytesReceived;
    double m_totalPower;
    uint64_t m_powerSamples;
};

NodeStatistics::NodeStatistics(Ptr<Node> node, uint32_t nodeId)
    : m_nodeId(nodeId), m_bytesReceived(0), m_totalPower(0), m_powerSamples(0) {
    m_stateTime[WIFI_PHY_IDLE] = Seconds(0);
    m_stateTime[WIFI_PHY_CCA_BUSY] = Seconds(0);
    m_stateTime[WIFI_PHY_TX] = Seconds(0);
    m_stateTime[WIFI_PHY_RX] = Seconds(0);
    m_lastUpdateTime = Now();

    Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(node->GetDevice(0));
    if (wifiDevice) {
        wifiDevice->GetPhy()->TraceConnectWithoutContext("PhyState", MakeCallback(&NodeStatistics::PhyStateTrace, this));
        wifiDevice->GetPhy()->TraceConnectWithoutContext("Rx", MakeCallback(&NodeStatistics::RxTrace, this));
        wifiDevice->GetPhy()->TraceConnectWithoutContext("Tx", MakeCallback(&NodeStatistics::TxTrace, this));
        wifiDevice->GetPhy()->TraceConnectWithoutContext("TxPower", MakeCallback(&NodeStatistics::UpdatePower, this));
    }
}

void NodeStatistics::PhyStateTrace(std::string context, Time now, WifiPhyState state) {
    Time duration = now - m_lastUpdateTime;
    m_stateTime[state] += duration;
    m_lastUpdateTime = now;
}

void NodeStatistics::RxTrace(Ptr<const Packet> packet, const Address &) {
    m_bytesReceived += packet->GetSize();
}

void NodeStatistics::TxTrace(Ptr<const Packet> packet) {}

void NodeStatistics::UpdatePower(double power) {
    m_totalPower += power;
    m_powerSamples++;
}

void NodeStatistics::PrintStats(Time interval) {
    NS_LOG_INFO("Node " << m_nodeId << " Statistics:");
    for (auto &entry : m_stateTime) {
        NS_LOG_INFO("  State " << entry.first << ": " << entry.second.GetSeconds() << "s");
    }
    NS_LOG_INFO("  Total Bytes Received: " << m_bytesReceived);
    NS_LOG_INFO("  Avg Transmit Power: " << (m_powerSamples > 0 ? m_totalPower / m_powerSamples : 0.0));
    NS_LOG_INFO("  Throughput: " << ((double)m_bytesReceived / Simulator::Now().GetSeconds()) * 8e-6 << " Mbps");

    Simulator::Schedule(interval, &NodeStatistics::PrintStats, this, interval);
}

void NodeStatistics::OutputGnuplotData(std::ofstream &outFile) {
    outFile << Simulator::Now().GetSeconds() << "\t";
    outFile << ((double)m_bytesReceived / Simulator::Now().GetSeconds()) * 8e-6 << "\t";
    outFile << (m_powerSamples > 0 ? m_totalPower / m_powerSamples : 0.0) << "\t";
    outFile << m_stateTime[WIFI_PHY_IDLE].GetSeconds() << "\t";
    outFile << m_stateTime[WIFI_PHY_CCA_BUSY].GetSeconds() << "\t";
    outFile << m_stateTime[WIFI_PHY_TX].GetSeconds() << "\t";
    outFile << m_stateTime[WIFI_PHY_RX].GetSeconds() << "\n";
}

static void PlotResults(const std::string &fileName) {
    Gnuplot plot(fileName + ".png");
    plot.SetTitle("Throughput, Power and WiFi States Over Time");
    plot.SetKey(Gnuplot::KeyInside, Gnuplot::Bottom, Gnuplot::Right);
    plot.SetTerminal("png size 1024,768");

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Throughput (Mbps)");
    dataset.SetStyle(Gnuplot2dDataset::LINES);
    dataset.AddEmptyLine();

    Gnuplot2dDataset dataset2;
    dataset2.SetTitle("Avg TX Power (dBm)");
    dataset2.SetStyle(Gnuplot2dDataset::LINES);
    dataset2.AddEmptyLine();

    Gnuplot2dDataset dataset3;
    dataset3.SetTitle("Idle (%)");
    dataset3.SetStyle(Gnuplot2dDataset::LINES);
    dataset3.AddEmptyLine();

    Gnuplot2dDataset dataset4;
    dataset4.SetTitle("CCA Busy (%)");
    dataset4.SetStyle(Gnuplot2dDataset::LINES);
    dataset4.AddEmptyLine();

    Gnuplot2dDataset dataset5;
    dataset5.SetTitle("TX (%)");
    dataset5.SetStyle(Gnuplot2dDataset::LINES);
    dataset5.AddEmptyLine();

    Gnuplot2dDataset dataset6;
    dataset6.SetTitle("RX (%)");
    dataset6.SetStyle(Gnuplot2dDataset::LINES);
    dataset6.AddEmptyLine();

    std::ifstream inFile(fileName.c_str(), std::ios::in);
    double x, y1, y2, y3, y4, y5, y6;
    while (inFile >> x >> y1 >> y2 >> y3 >> y4 >> y5 >> y6) {
        dataset.Add(x, y1);
        dataset2.Add(x, y2);
        dataset3.Add(x, y3);
        dataset4.Add(x, y4);
        dataset5.Add(x, y5);
        dataset6.Add(x, y6);
    }

    plot.AddDataset(dataset);
    plot.AddDataset(dataset2);
    plot.AddDataset(dataset3);
    plot.AddDataset(dataset4);
    plot.AddDataset(dataset5);
    plot.AddDataset(dataset6);

    std::ofstream plotFile(fileName + ".plt");
    plot.GenerateOutput(plotFile);
}

int main(int argc, char *argv[]) {
    std::string managerType = "ns3::ParfWifiManager";
    uint32_t rtsThreshold = 2346;
    int64_t txPowerLevel = 16; // dBm
    double simulationTime = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("manager", "WiFi rate control manager type", managerType);
    cmd.AddValue("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
    cmd.AddValue("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
    cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
    Config::SetDefault("ns3::WifiPhy::TxPowerStart", DoubleValue(txPowerLevel));
    Config::SetDefault("ns3::WifiPhy::TxPowerEnd", DoubleValue(txPowerLevel));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate54Mbps"));

    NodeContainer apNodes, staNodes;
    apNodes.Create(2);
    staNodes.Create(2);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    positionAlloc->Add(Vector(0.0, 5.0, 0.0));
    positionAlloc->Add(Vector(10.0, 5.0, 0.0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + managerType.substr(managerType.find("::") + 2), "DataMode", StringValue("OfdmRate54Mbps"));

    Ssid ssid1 = Ssid("network-1");
    Ssid ssid2 = Ssid("network-2");

    NetDeviceContainer apDevices1 = wifi.Install(phy, apNodes.Get(0));
    NetDeviceContainer apDevices2 = wifi.Install(phy, apNodes.Get(1));
    NetDeviceContainer staDevices1 = wifi.Install(phy, staNodes.Get(0));
    NetDeviceContainer staDevices2 = wifi.Install(phy, staNodes.Get(1));

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));

    ApplicationContainer sinkApps1 = sink1.Install(staNodes.Get(0));
    ApplicationContainer sinkApps2 = sink2.Install(staNodes.Get(1));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(simulationTime));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(simulationTime));

    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces1.GetAddress(0), 9));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onoff1.SetAttribute("PacketSize", UintegerValue(1472));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces2.GetAddress(0), 9));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onoff2.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer cbrApps1 = onoff1.Install(apNodes.Get(0));
    ApplicationContainer cbrApps2 = onoff2.Install(apNodes.Get(1));
    cbrApps1.Start(Seconds(1.0));
    cbrApps1.Stop(Seconds(simulationTime));
    cbrApps2.Start(Seconds(1.0));
    cbrApps2.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    std::vector<Ptr<NodeStatistics>> stats;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        stats.push_back(CreateObject<NodeStatistics>(staNodes.Get(i), staNodes.Get(i)->GetId()));
    }

    Time printInterval = Seconds(1.0);
    for (auto &stat : stats) {
        stat->PrintStats(printInterval);
    }

    std::string dataFileName = "wifi_simulation_data.dat";
    std::ofstream outFile(dataFileName.c_str());
    outFile << "# Time(s)\tThroughput(Mbps)\tAvgPower(dBm)\tIdle(s)\tCCA_Busy(s)\tTX(s)\tRX(s)\n";

    Simulator::Schedule(Seconds(1.0), [&, &outFile]() {
        for (auto &stat : stats) {
            stat->OutputGnuplotData(outFile);
        }
        if (Simulator::Now().GetSeconds() < simulationTime) {
            Simulator::Schedule(Seconds(1.0), [&]() {});
        } else {
            outFile.close();
            PlotResults(dataFileName);
        }
    });

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> statsMap = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = statsMap.begin(); i != statsMap.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1e6 << " Mbps\n";
        std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
        std::cout << "  Jitter: " << i->second.jitterSum.GetSeconds() / (i->second.rxPackets > 1 ? i->second.rxPackets - 1 : 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}