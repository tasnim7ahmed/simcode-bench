#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiSimulation");

class NodeStatistics {
public:
    NodeStatistics(Ptr<Node> node);
    void PhyStateTrace(std::string context, Time start, Time duration, WifiPhyState state);
    void PacketSinkRx(Ptr<const Packet> packet, const Address& from);
    void PhyTxBegin(std::string context, Ptr<const Packet> packet, double txPowerW);
    void Report();
    void PrintGnuplot(std::ostream &out);

private:
    Ptr<Node> m_node;
    std::map<WifiPhyState, Time> m_stateTime;
    uint64_t m_totalRxBytes;
    double m_avgTxPowerSum;
    uint64_t m_txCount;
};

NodeStatistics::NodeStatistics(Ptr<Node> node)
    : m_node(node),
      m_totalRxBytes(0),
      m_avgTxPowerSum(0.0),
      m_txCount(0) {
    for (auto state : {WIFI_PHY_STATE_CCA_BUSY, WIFI_PHY_STATE_IDLE, WIFI_PHY_STATE_TX, WIFI_PHY_STATE_RX}) {
        m_stateTime[state] = Seconds(0);
    }
}

void NodeStatistics::PhyStateTrace(std::string context, Time start, Time duration, WifiPhyState state) {
    if (m_stateTime.find(state) != m_stateTime.end()) {
        m_stateTime[state] += duration;
    }
}

void NodeStatistics::PacketSinkRx(Ptr<const Packet> packet, const Address& from) {
    m_totalRxBytes += packet->GetSize();
}

void NodeStatistics::PhyTxBegin(std::string context, Ptr<const Packet> packet, double txPowerW) {
    m_avgTxPowerSum += 10 * log10(txPowerW) + 30; // Convert W to dBm
    m_txCount++;
}

void NodeStatistics::Report() {
    double avgTxPower = m_txCount > 0 ? m_avgTxPowerSum / m_txCount : 0.0;
    double throughput = (m_totalRxBytes * 8.0) / 1e6; // Mbps over simulation time

    NS_LOG_UNCOND("Node " << m_node->GetId()
                          << ": Total Rx Bytes=" << m_totalRxBytes
                          << ", Throughput=" << throughput << " Mbps"
                          << ", Avg Tx Power=" << avgTxPower << " dBm");
    NS_LOG_UNCOND("  CCA Busy Time: " << m_stateTime[WIFI_PHY_STATE_CCA_BUSY].As(Time::S));
    NS_LOG_UNCOND("  Idle Time:     " << m_stateTime[WIFI_PHY_STATE_IDLE].As(Time::S));
    NS_LOG_UNCOND("  TX Time:       " << m_stateTime[WIFI_PHY_STATE_TX].As(Time::S));
    NS_LOG_UNCOND("  RX Time:       " << m_stateTime[WIFI_PHY_STATE_RX].As(Time::S));
}

void NodeStatistics::PrintGnuplot(std::ostream &out) {
    out << Simulator::Now().GetSeconds() << " "
        << (m_totalRxBytes * 8.0) / (Simulator::Now().GetSeconds() * 1e6) << " "
        << (m_txCount > 0 ? m_avgTxPowerSum / m_txCount : 0.0) << " "
        << m_stateTime[WIFI_PHY_STATE_CCA_BUSY].GetSeconds() << " "
        << m_stateTime[WIFI_PHY_STATE_IDLE].GetSeconds() << " "
        << m_stateTime[WIFI_PHY_STATE_TX].GetSeconds() << " "
        << m_stateTime[WIFI_PHY_STATE_RX].GetSeconds() << std::endl;
}

static void PlotResults(const std::vector<std::shared_ptr<NodeStatistics>> &statsList, const std::string &filename) {
    Gnuplot plot(filename);
    plot.SetTitle("Network Metrics Over Time");
    plot.SetLegend("Time (s)", "Metric Value");

    Gnuplot2dDataset datasetThroughput;
    datasetThroughput.SetTitle("Throughput (Mbps)");
    datasetThroughput.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot2dDataset datasetTxPower;
    datasetTxPower.SetTitle("Avg Tx Power (dBm)");
    datasetTxPower.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot2dDataset datasetCcaBusy;
    datasetCcaBusy.SetTitle("CCA Busy Time (s)");
    datasetCcaBusy.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot2dDataset datasetIdle;
    datasetIdle.SetTitle("Idle Time (s)");
    datasetIdle.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot2dDataset datasetTx;
    datasetTx.SetTitle("TX Time (s)");
    datasetTx.SetStyle(Gnuplot2dDataset::LINES);

    Gnuplot2dDataset datasetRx;
    datasetRx.SetTitle("RX Time (s)");
    datasetRx.SetStyle(Gnuplot2dDataset::LINES);

    for (const auto &stat : statsList) {
        std::ostringstream oss;
        stat->PrintGnuplot(oss);
        double t, th, txp, cca, idle, tx, rx;
        while (oss >> t >> th >> txp >> cca >> idle >> tx >> rx) {
            datasetThroughput.Add(t, th);
            datasetTxPower.Add(t, txp);
            datasetCcaBusy.Add(t, cca);
            datasetIdle.Add(t, idle);
            datasetTx.Add(t, tx);
            datasetRx.Add(t, rx);
        }
    }

    plot.AddDataset(datasetThroughput);
    plot.AddDataset(datasetTxPower);
    plot.AddDataset(datasetCcaBusy);
    plot.AddDataset(datasetIdle);
    plot.AddDataset(datasetTx);
    plot.AddDataset(datasetRx);

    std::ofstream plotFile(filename.c_str());
    plot.GenerateOutput(plotFile);
    plotFile.close();
}

int main(int argc, char *argv[]) {
    std::string wifiManagerType = "ns3::ParfWifiManager";
    uint32_t rtsThreshold = 2346;
    int64_t txPowerLevel = 16; // dBm
    double simTime = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("wifiManager", "Type of WiFi manager", wifiManagerType);
    cmd.AddValue("rtsThreshold", "RTS threshold value", rtsThreshold);
    cmd.AddValue("txPower", "Transmit power level (dBm)", txPowerLevel);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
    Config::SetDefault(wifiManagerType, StringValue(""));
    Config::SetDefault("ns3::YansWifiPhy::TxPowerStart", DoubleValue(txPowerLevel));
    Config::SetDefault("ns3::YansWifiPhy::TxPowerEnd", DoubleValue(txPowerLevel));

    NodeContainer apNodes, staNodes;
    apNodes.Create(2);
    staNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::ListPositionAllocator");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + wifiManagerType.substr(wifiManagerType.find("::") + 2));

    Ssid ssid1 = Ssid("network-1");
    Ssid ssid2 = Ssid("network-2");

    WifiMacHelper mac;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));

    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes.Get(0));
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes.Get(1));

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    address.NewNetwork();
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    uint16_t port = 9;
    ApplicationContainer sinkApps1, sinkApps2;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    sinkApps1 = sinkHelper.Install(staNodes.Get(0));
    sinkApps2 = sinkHelper.Install(staNodes.Get(1));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(simTime));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(simTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces1.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("54Mbps"));
    ApplicationContainer apps1 = onoff.Install(apNodes.Get(0));
    apps1.Start(Seconds(0.0));
    apps1.Stop(Seconds(simTime));

    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(staInterfaces2.GetAddress(0), port)));
    ApplicationContainer apps2 = onoff.Install(apNodes.Get(1));
    apps2.Start(Seconds(0.0));
    apps2.Stop(Seconds(simTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    std::vector<std::shared_ptr<NodeStatistics>> statsList;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        auto stats = std::make_shared<NodeStatistics>(staNodes.Get(i));
        statsList.push_back(stats);

        Config::ConnectWithoutContext("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/Phy/State/Duration", 
                                      MakeCallback(&NodeStatistics::PhyStateTrace, stats.get()));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/ApplicationList/*/Rx", 
                                      MakeCallback(&NodeStatistics::PacketSinkRx, stats.get()));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/*/Phy/TxBegin", 
                                      MakeCallback(&NodeStatistics::PhyTxBegin, stats.get()));

        Simulator::Schedule(Seconds(1.0), &NodeStatistics::Report, stats.get());
        Simulator::Schedule(Seconds(1.0), &PlotResults, statsList, "wifi-metrics.plot");
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND("  Tx Packets = " << iter->second.txPackets);
        NS_LOG_UNCOND("  Rx Packets = " << iter->second.rxPackets);
        NS_LOG_UNCOND("  Lost Packets = " << iter->second.lostPackets);
        NS_LOG_UNCOND("  Throughput: " << iter->second.rxBytes * 8.0 / simTime / 1000 / 1000 << " Mbps");
        NS_LOG_UNCOND("  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s");
        NS_LOG_UNCOND("  Mean Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s");
    }

    Simulator::Destroy();
    return 0;
}