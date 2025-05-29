#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <numeric>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulation");

class NodeStatistics {
public:
    NodeStatistics(Ptr<Node> node, std::string prefix, double interval = 1.0)
        : m_node(node), m_prefix(prefix), m_interval(interval),
          m_totalBytesReceived(0), m_totalTime(0), m_averageTransmitPower(0), m_numSamples(0)
    {
        m_lastRx = Simulator::Now();
        m_lastTx = Simulator::Now();
        m_lastIdle = Simulator::Now();
        m_lastCcaBusy = Simulator::Now();
        m_timeRx = Seconds(0.0);
        m_timeTx = Seconds(0.0);
        m_timeIdle = Seconds(0.0);
        m_timeCcaBusy = Seconds(0.0);
        m_rxTrace = Names::FindName(node) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx";
        m_txTrace = Names::FindName(node) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx";
        m_phyTxBeginTrace = Names::FindName(node) + "/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin";
        m_phyRxBeginTrace = Names::FindName(node) + "/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin";
        m_ccaBusyTrace = Names::FindName(node) + "/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop";
        m_transmitPowerTrace = Names::FindName(node) + "/DeviceList/*/$ns3::WifiNetDevice/Phy/State/Tx";
    }

    void Install() {
        Config::ConnectWithoutContext(m_rxTrace, MakeCallback(&NodeStatistics::RxEvent, this));
        Config::ConnectWithoutContext(m_txTrace, MakeCallback(&NodeStatistics::TxEvent, this));
        Config::ConnectWithoutContext(m_phyTxBeginTrace, MakeCallback(&NodeStatistics::PhyTxBeginEvent, this));
        Config::ConnectWithoutContext(m_phyRxBeginTrace, MakeCallback(&NodeStatistics::PhyRxBeginEvent, this));
        Config::ConnectWithoutContext(m_ccaBusyTrace, MakeCallback(&NodeStatistics::CcaBusyEvent, this));
        Config::ConnectWithoutContext(m_transmitPowerTrace, MakeCallback(&NodeStatistics::TransmitPowerEvent, this));
        ScheduleNextPrint();
    }

    void RxEvent(std::string context, Ptr<const Packet> packet) {
        m_totalBytesReceived += packet->GetSize();
        m_lastRx = Simulator::Now();
    }

    void TxEvent(std::string context, Ptr<const Packet> packet) {
        m_lastTx = Simulator::Now();
    }

    void PhyTxBeginEvent(std::string context, uint32_t rate) {
        m_lastTx = Simulator::Now();
    }

    void PhyRxBeginEvent(std::string context, uint32_t rate, double gain) {
        m_lastRx = Simulator::Now();
    }

     void CcaBusyEvent(std::string context, Ptr<const Packet> packet, WifiPhyRxDropReason reason) {
        m_lastCcaBusy = Simulator::Now();
    }

    void TransmitPowerEvent(std::string context, double powerDbm) {
        m_averageTransmitPower += powerDbm;
        m_numSamples++;
    }

    void PrintStatistics() {
        Time now = Simulator::Now();

        Time deltaRx = now - m_lastRx;
        Time deltaTx = now - m_lastTx;
        Time deltaIdle = now - m_lastIdle;
        Time deltaCcaBusy = now - m_lastCcaBusy;

        m_timeRx += deltaRx;
        m_timeTx += deltaTx;
        m_timeIdle += deltaIdle;
        m_timeCcaBusy += deltaCcaBusy;

        double throughput = (m_totalBytesReceived * 8.0) / (m_interval * 1e6); // Mbps
        double avgPower = (m_numSamples > 0) ? (m_averageTransmitPower / m_numSamples) : 0;

        std::cout << now.GetSeconds() << " "
                  << throughput << " "
                  << avgPower << " "
                  << m_timeRx.GetSeconds() << " "
                  << m_timeTx.GetSeconds() << " "
                  << m_timeIdle.GetSeconds() << " "
                  << m_timeCcaBusy.GetSeconds()
                  << std::endl;

        m_totalBytesReceived = 0;
        m_averageTransmitPower = 0;
        m_numSamples = 0;
        m_lastRx = now;
        m_lastTx = now;
        m_lastIdle = now;
        m_lastCcaBusy = now;

        ScheduleNextPrint();
    }

private:
    void ScheduleNextPrint() {
        Simulator::Schedule(Seconds(m_interval), &NodeStatistics::PrintStatistics, this);
    }

    Ptr<Node> m_node;
    std::string m_prefix;
    double m_interval;
    uint64_t m_totalBytesReceived;
    Time m_lastRx;
    Time m_lastTx;
    Time m_lastIdle;
    Time m_lastCcaBusy;
    Time m_timeRx;
    Time m_timeTx;
    Time m_timeIdle;
    Time m_timeCcaBusy;
    double m_totalTime;
    double m_averageTransmitPower;
    uint32_t m_numSamples;

    std::string m_rxTrace;
    std::string m_txTrace;
    std::string m_phyTxBeginTrace;
    std::string m_phyRxBeginTrace;
    std::string m_ccaBusyTrace;
    std::string m_transmitPowerTrace;
};

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nStas = 2;
    std::string wifiManagerType = "ns3::ParfWifiManager";
    int rtsCtsThreshold = 2347;
    double txPowerLevel = 16.0206; // dBm, equivalent to 40mW
    double simulationTime = 100;

    // Define positions for APs and STAs (can be overridden via command line)
    Vector ap1Pos(10.0, 10.0, 0.0);
    Vector ap2Pos(40.0, 10.0, 0.0);
    Vector sta1Pos(10.0, 20.0, 0.0);
    Vector sta2Pos(40.0, 20.0, 0.0);

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("nStas", "Number of STA nodes", nStas);
    cmd.AddValue("wifiManagerType", "Type of WifiManager: ns3::ParfWifiManager or ns3::ArfWifiManager", wifiManagerType);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold", rtsCtsThreshold);
    cmd.AddValue("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("ap1X", "X position of AP1", ap1Pos.x);
    cmd.AddValue("ap1Y", "Y position of AP1", ap1Pos.y);
    cmd.AddValue("ap2X", "X position of AP2", ap2Pos.x);
    cmd.AddValue("ap2Y", "Y position of AP2", ap2Pos.y);
    cmd.AddValue("sta1X", "X position of STA1", sta1Pos.x);
    cmd.AddValue("sta1Y", "Y position of STA1", sta1Pos.y);
    cmd.AddValue("sta2X", "X position of STA2", sta2Pos.x);
    cmd.AddValue("sta2Y", "Y position of STA2", sta2Pos.y);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiSimulation", LOG_LEVEL_INFO);
    }

    NodeContainer apNodes, staNodes;
    apNodes.Create(2);
    staNodes.Create(2);

    // Configure YansWifiChannelHelper
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(txPowerLevel));
    phy.Set("TxPowerEnd", DoubleValue(txPowerLevel));

    // Configure WifiHelper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
    wifi.SetRemoteStationManager(wifiManagerType);

    // Configure different SSIDs for each AP-STA pair
    Ssid ssid1 = Ssid("ns3-ssid-1");
    Ssid ssid2 = Ssid("ns3-ssid-2");

    // Configure AP devices
    WifiMacHelper macAp;
    macAp.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid1),
                  "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice1 = wifi.Install(phy, macAp, apNodes.Get(0));

    macAp.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid2),
                  "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice2 = wifi.Install(phy, macAp, apNodes.Get(1));

    // Configure STA devices
    WifiMacHelper macSta;
    macSta.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid1),
                   "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice1 = wifi.Install(phy, macSta, staNodes.Get(0));

     macSta.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid2),
                   "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice2 = wifi.Install(phy, macSta, staNodes.Get(1));

    // Set RTS/CTS threshold
    Config::SetDefault("ns3::WifiMacQueue::RtsCtsThreshold", UintegerValue(rtsCtsThreshold));

    // Install mobility models
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(ap1Pos);
    positionAlloc->Add(ap2Pos);
    positionAlloc->Add(sta1Pos);
    positionAlloc->Add(sta2Pos);
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevice1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevice1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevice2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevice2);

    // Install PacketSink on STAs
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp1 = sinkHelper.Install(staNodes.Get(0));
    sinkApp1.Start(Seconds(1.0));
    sinkApp1.Stop(Seconds(simulationTime));

    ApplicationContainer sinkApp2 = sinkHelper.Install(staNodes.Get(1));
    sinkApp2.Start(Seconds(1.0));
    sinkApp2.Stop(Seconds(simulationTime));

    // Configure CBR traffic from APs to STAs
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces1.GetAddress(0), port));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer cbrApp1 = onOffHelper.Install(apNodes.Get(0));
    cbrApp1.Start(Seconds(2.0));
    cbrApp1.Stop(Seconds(simulationTime));

    OnOffHelper onOffHelper2("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces2.GetAddress(0), port));
    onOffHelper2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper2.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onOffHelper2.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer cbrApp2 = onOffHelper2.Install(apNodes.Get(1));
    cbrApp2.Start(Seconds(2.0));
    cbrApp2.Stop(Seconds(simulationTime));

    // Install FlowMonitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Create and install NodeStatistics
    NodeStatistics stats1(staNodes.Get(0), "STA1");
    stats1.Install();
    NodeStatistics stats2(staNodes.Get(1), "STA2");
    stats2.Install();

    // Set up Gnuplot
    std::string fileNameWithNoExtension = "wifi-simulation";
    std::string graphicsFileName = fileNameWithNoExtension + ".png";
    std::string plotFileName = fileNameWithNoExtension + ".plt";
    std::string dataFileName = fileNameWithNoExtension + ".dat";

    Gnuplot plot(graphicsFileName);
    plot.SetTitle("WiFi Simulation Results");
    plot.SetLegend("Time (s)", "Throughput (Mbps)");

    Gnuplot2dDataset dataset;
    dataset.SetTitle("STA1 Throughput");
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    std::ofstream out(dataFileName.c_str());
    out << "#Time Throughput(Mbps) AvgTxPower(dBm) RxTime TxTime IdleTime CcaBusyTime" << std::endl;
    out.close();

    std::ofstream plotFile(plotFileName.c_str());
    plotFile << "set terminal png size 1024,768" << std::endl;
    plotFile << "set output \"" << graphicsFileName << "\"" << std::endl;
    plotFile << "set title \"WiFi Simulation Results\"" << std::endl;
    plotFile << "set xlabel \"Time (s)\"" << std::endl;
    plotFile << "set ylabel \"Throughput (Mbps)\"" << std::endl;
    plotFile << "plot \"" << dataFileName << "\" using 1:2 title 'Throughput' with linespoints" << std::endl;
    plotFile.close();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        std::cout << "  Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
        std::cout << "  Jitter: " << i->second.jitterSum.GetSeconds() / (i->second.rxPackets - 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}