#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulation");

class NodeStatistics {
public:
    NodeStatistics(NodeContainer nodes, double interval, std::string filenamePrefix)
        : m_nodes(nodes), m_interval(Seconds(interval)), m_filenamePrefix(filenamePrefix) {
        m_wifiPhy = DynamicCast<YansWifiPhy>(m_nodes.Get(0)->GetDevice(0)->GetNetDevice()->GetPhy());
        m_ccaBusyTime.resize(m_nodes.GetN(), Seconds(0));
        m_idleTime.resize(m_nodes.GetN(), Seconds(0));
        m_txTime.resize(m_nodes.GetN(), Seconds(0));
        m_rxTime.resize(m_nodes.GetN(), Seconds(0));
        m_lastStateTime.resize(m_nodes.GetN(), Simulator::Now());
        m_totalBytesReceived.resize(m_nodes.GetN(), 0);
        m_totalPacketsReceived.resize(m_nodes.GetN(), 0);
        m_totalTransmitPower.resize(m_nodes.GetN(), 0);
        m_samplesTransmitPower.resize(m_nodes.GetN(), 0);
        m_lastBytesReceived.resize(m_nodes.GetN(), 0);

        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice>(m_nodes.Get(i)->GetDevice(0));
            Ptr<WifiMac> wifiMac = wifiNetDevice->GetMac();
            wifiMac->TraceConnectWithoutContext("MacTx", MakeCallback(&NodeStatistics::MacTxTrace, this));
        }

        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice>(m_nodes.Get(i)->GetDevice(0));
            Ptr<WifiPhy> wifiPhy = wifiNetDevice->GetPhy();
            wifiPhy->TraceConnectWithoutContext("CCAState", MakeBoundCallback(&NodeStatistics::CcaStateTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("RxOk", MakeBoundCallback(&NodeStatistics::RxOkTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("Tx", MakeBoundCallback(&NodeStatistics::TxTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("Rx", MakeBoundCallback(&NodeStatistics::RxTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("TxBegin", MakeBoundCallback(&NodeStatistics::TxBeginTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("TxEnd", MakeBoundCallback(&NodeStatistics::TxEndTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("RxBegin", MakeBoundCallback(&NodeStatistics::RxBeginTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("RxEnd", MakeBoundCallback(&NodeStatistics::RxEndTrace, this, i));
            wifiPhy->TraceConnectWithoutContext("TxPowerDbm", MakeBoundCallback(&NodeStatistics::TxPowerTrace, this, i));
        }

        Simulator::Schedule(m_interval, &NodeStatistics::ReportStatistics, this);
    }

private:
    void CcaStateTrace(uint32_t nodeId, WifiPhy::CcaState state) {
        Time now = Simulator::Now();
        Time delta = now - m_lastStateTime[nodeId];

        if (state == WifiPhy::CCA_BUSY) {
            m_idleTime[nodeId] += delta;
        } else if (state == WifiPhy::CCA_IDLE) {
            m_ccaBusyTime[nodeId] += delta;
        }
        m_lastStateTime[nodeId] = now;
    }

    void RxOkTrace(uint32_t nodeId, Ptr<const Packet> packet, double snr, WifiMode mode, enum WifiPreamble preamble) {
        m_totalBytesReceived[nodeId] += packet->GetSize();
        m_totalPacketsReceived[nodeId]++;
    }

    void TxTrace(uint32_t nodeId, Ptr<const Packet> packet, WifiMode mode, enum WifiPreamble preamble) {}
    void RxTrace(uint32_t nodeId, uint32_t packets) {}
    void TxBeginTrace(uint32_t nodeId, Ptr<const Packet> packet) {}
    void TxEndTrace(uint32_t nodeId, Ptr<const Packet> packet) {}
    void RxBeginTrace(uint32_t nodeId, Ptr<const Packet> packet) {}
    void RxEndTrace(uint32_t nodeId, Ptr<const Packet> packet) {}

    void TxPowerTrace(uint32_t nodeId, double power) {
        m_totalTransmitPower[nodeId] += power;
        m_samplesTransmitPower[nodeId]++;
    }

    void MacTxTrace(Ptr<const Packet> packet) {}

    void ReportStatistics() {
        std::vector<double> throughput(m_nodes.GetN());
        std::vector<double> averagePower(m_nodes.GetN());

        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            uint64_t bytesReceived = m_totalBytesReceived[i] - m_lastBytesReceived[i];
            throughput[i] = (double)bytesReceived * 8 / m_interval.GetSeconds() / 1000000;
            m_lastBytesReceived[i] = m_totalBytesReceived[i];

            if (m_samplesTransmitPower[i] > 0) {
                averagePower[i] = m_totalTransmitPower[i] / m_samplesTransmitPower[i];
            } else {
                averagePower[i] = 0;
            }
        }

        PrintStatistics(throughput, averagePower);
        Simulator::Schedule(m_interval, &NodeStatistics::ReportStatistics, this);
    }

    void PrintStatistics(std::vector<double> throughput, std::vector<double> averagePower) {
        std::ofstream ofs;
        ofs.open(m_filenamePrefix + ".dat", std::ios_base::app);
        ofs << Simulator::Now().GetSeconds();
        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            ofs << " " << throughput[i] << " " << averagePower[i]
                << " " << m_ccaBusyTime[i].GetSeconds() << " " << m_idleTime[i].GetSeconds()
                << " " << m_txTime[i].GetSeconds() << " " << m_rxTime[i].GetSeconds();
        }
        ofs << std::endl;
        ofs.close();
    }

public:
    void CreateGnuplotFiles() {
        std::string filename = m_filenamePrefix + ".gp";
        std::ofstream gpfile(filename.c_str());
        gpfile << "set terminal png size 1024,768" << std::endl;
        gpfile << "set output \"" << m_filenamePrefix + ".png" << "\"" << std::endl;
        gpfile << "set title \"Wifi Simulation\"" << std::endl;
        gpfile << "set xlabel \"Time (s)\"" << std::endl;

        gpfile << "set ylabel \"Throughput (Mbps)\"" << std::endl;
        gpfile << "plot \"" << m_filenamePrefix + ".dat\" using 1:2 title 'STA0 Throughput' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:4 title 'STA1 Throughput' with lines" << std::endl;

        gpfile << "set ylabel \"Average Transmit Power (dBm)\"" << std::endl;
        gpfile << "plot \"" << m_filenamePrefix + ".dat\" using 1:3 title 'STA0 Power' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:5 title 'STA1 Power' with lines" << std::endl;

        gpfile << "set ylabel \"Time (s)\"" << std::endl;
        gpfile << "plot \"" << m_filenamePrefix + ".dat\" using 1:6 title 'STA0 CCA Busy' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:7 title 'STA0 Idle' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:8 title 'STA0 TX' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:9 title 'STA0 RX' with lines" << std::endl;
        gpfile << "replot \"" << m_filenamePrefix + ".dat\" using 1:10 title 'STA1 CCA Busy' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:11 title 'STA1 Idle' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:12 title 'STA1 TX' with lines, ";
        gpfile << "\"" << m_filenamePrefix + ".dat\" using 1:13 title 'STA1 RX' with lines" << std::endl;

        gpfile << "set terminal dumb" << std::endl;
        gpfile.close();
    }

private:
    NodeContainer m_nodes;
    Time m_interval;
    std::string m_filenamePrefix;
    Ptr<YansWifiPhy> m_wifiPhy;
    std::vector<Time> m_ccaBusyTime;
    std::vector<Time> m_idleTime;
    std::vector<Time> m_txTime;
    std::vector<Time> m_rxTime;
    std::vector<Time> m_lastStateTime;
    std::vector<uint64_t> m_totalBytesReceived;
    std::vector<uint32_t> m_totalPacketsReceived;
    std::vector<double> m_totalTransmitPower;
    std::vector<uint32_t> m_samplesTransmitPower;
    std::vector<uint64_t> m_lastBytesReceived;
};

int main(int argc, char *argv[]) {
    bool verbose = false;
    double simulationTime = 100.0;
    double interval = 1.0;
    std::string wifiManagerType = "ns3::ParfWifiManager";
    uint32_t rtsCtsThreshold = 2347;
    double txPowerLevel = 20.0;
    std::string dataRate = "OfdmRate54Mbps";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log ifconfig interface output", verbose);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("interval", "Statistics reporting interval in seconds", interval);
    cmd.AddValue("wifiManagerType", "The type of wifi manager to use", wifiManagerType);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold in bytes", rtsCtsThreshold);
    cmd.AddValue("txPowerLevel", "Transmit power level in dBm", txPowerLevel);
    cmd.AddValue("dataRate", "CBR traffic data rate", dataRate);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiSimulation", LOG_LEVEL_INFO);
    }

    NodeContainer apNodes;
    apNodes.Create(2);
    NodeContainer staNodes;
    staNodes.Create(2);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211a);
    wifiHelper.SetRemoteStationManager(wifiManagerType);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiPhy.Set("TxPowerStart", DoubleValue(txPowerLevel));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerLevel));

    WifiMacHelper macHelper;
    Ssid ssid1 = Ssid("ns-3-ssid-1");
    Ssid ssid2 = Ssid("ns-3-ssid-2");

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifiHelper.Install(wifiPhy, macHelper, staNodes.Get(0));

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifiHelper.Install(wifiPhy, macHelper, staNodes.Get(1));

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "BeaconInterval", TimeValue(Seconds(0.05)));
    NetDeviceContainer apDevices1 = wifiHelper.Install(wifiPhy, macHelper, apNodes.Get(0));

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "BeaconInterval", TimeValue(Seconds(0.05)));
    NetDeviceContainer apDevices2 = wifiHelper.Install(wifiPhy, macHelper, apNodes.Get(1));

    NetDeviceContainer apStaDevices1 = NetDeviceContainer(apDevices1, staDevices1);
    NetDeviceContainer apStaDevices2 = NetDeviceContainer(apDevices2, staDevices2);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // AP0
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));  // AP1
    positionAlloc->Add(Vector(1.0, 5.0, 0.0));  // STA0
    positionAlloc->Add(Vector(6.0, 5.0, 0.0));  // STA1
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apStaInterfaces1 = address.Assign(apStaDevices1);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apStaInterfaces2 = address.Assign(apStaDevices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp1 = sinkHelper.Install(staNodes.Get(0));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(simulationTime));
    ApplicationContainer sinkApp2 = sinkHelper.Install(staNodes.Get(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(simulationTime));

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(apStaInterfaces1.GetAddress(1), port));
    onOffHelper.SetConstantRate(DataRate(dataRate));
    ApplicationContainer sourceApp1 = onOffHelper.Install(apNodes.Get(0));
    sourceApp1.Start(Seconds(1.0));
    sourceApp1.Stop(Seconds(simulationTime));

    OnOffHelper onOffHelper2("ns3::UdpSocketFactory", InetSocketAddress(apStaInterfaces2.GetAddress(1), port));
    onOffHelper2.SetConstantRate(DataRate(dataRate));
    ApplicationContainer sourceApp2 = onOffHelper2.Install(apNodes.Get(1));
    sourceApp2.Start(Seconds(1.0));
    sourceApp2.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    NodeStatistics stats(staNodes, interval, "wifi-stats");
    stats.CreateGnuplotFiles();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer statsContainer = flowMonitor->GetFlowStats();

    for (auto it = statsContainer.begin(); it != statsContainer.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << it->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << it->second.rxBytes << "\n";
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        std::cout << "  Delay:      " << it->second.delaySum.GetSeconds() / it->second.rxPackets << " s\n";
        std::cout << "  Jitter:     " << it->second.jitterSum.GetSeconds() / (it->second.rxPackets - 1) << " s\n";
    }

    flowMonitor->SerializeToXmlFile("wifi-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}