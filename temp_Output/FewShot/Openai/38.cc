#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DualApStaWifiStats");

class NodeStatistics : public Object
{
public:
    NodeStatistics(Ptr<Node> node, Ptr<NetDevice> dev, Ptr<WifiPhy> phy)
        : m_node(node), m_dev(dev), m_phy(phy),
          m_bytesRecv(0), m_lastRecv(0),
          m_totalTxPower(0.0), m_txPowerSamples(0)
    {
        m_stateTime.fill(0.0);
        m_lastPhyState = phy->GetState();
        m_lastStateChange = Simulator::Now();
        phy->TraceConnectWithoutContext("State", MakeCallback(&NodeStatistics::PhyStateChange, this));
        phy->TraceConnectWithoutContext("TxPowerStart", MakeCallback(&NodeStatistics::PhyTxPower, this));
    }

    void PhyStateChange(Time start, Time duration, WifiPhyState oldState, WifiPhyState newState)
    {
        Time now = Simulator::Now();
        double diff = (now - m_lastStateChange).GetSeconds();
        m_stateTime[m_lastPhyState] += diff;
        m_lastPhyState = newState;
        m_lastStateChange = now;
    }

    void PhyTxPower(double txPower)
    {
        m_totalTxPower += txPower;
        m_txPowerSamples++;
    }

    void PacketReceived(Ptr<const Packet> packet, const Address &from)
    {
        m_bytesRecv += packet->GetSize();
    }

    void ResetSecond()
    {
        m_lastRecv = m_bytesRecv;
        m_totalTxPower = 0.0;
        m_txPowerSamples = 0;
        m_lastStateTime = m_stateTime;
        m_stateTime.fill(0.0);
    }

    uint64_t GetBytesReceivedThisSecond() const { return m_bytesRecv - m_lastRecv; }
    uint64_t GetBytesReceivedTotal() const { return m_bytesRecv; }
    double GetAvgTxPower() const { return m_txPowerSamples > 0 ? m_totalTxPower / m_txPowerSamples : 0.0; }
    std::array<double, 4> GetStateTime() const { return m_lastStateTime; }

private:
    Ptr<Node> m_node;
    Ptr<NetDevice> m_dev;
    Ptr<WifiPhy> m_phy;
    uint64_t m_bytesRecv;
    uint64_t m_lastRecv;
    double m_totalTxPower;
    uint32_t m_txPowerSamples;
    std::array<double, 4> m_stateTime;
    std::array<double, 4> m_lastStateTime;
    WifiPhyState m_lastPhyState;
    Time m_lastStateChange;
};

enum {
    STA_IDX_A = 0,
    STA_IDX_B = 1,
    AP_IDX_A = 0,
    AP_IDX_B = 1
};

static std::vector<Ptr<NodeStatistics>> g_staStats;

void PrintStats(Gnuplot2dDataset &throughputDataset,
                Gnuplot2dDataset &powerDataset,
                std::vector<Gnuplot2dDataset> &stateDatasets,
                double intervalSeconds,
                double &lastTime)
{
    double nowSec = Simulator::Now().GetSeconds();
    lastTime = nowSec;
    for (size_t i = 0; i < g_staStats.size(); ++i)
    {
        uint64_t bytes = g_staStats[i]->GetBytesReceivedThisSecond();
        double throughput = bytes * 8.0 / 1e6 / intervalSeconds; // Mbps
        double avgTxPower = g_staStats[i]->GetAvgTxPower();
        std::array<double, 4> states = g_staStats[i]->GetStateTime();
        throughputDataset.Add(nowSec, throughput);
        powerDataset.Add(nowSec, avgTxPower);

        for (size_t j = 0; j < 4; ++j)
            stateDatasets[j].Add(nowSec, states[j] / intervalSeconds);
        g_staStats[i]->ResetSecond();
        std::cout << "Time " << nowSec << "s, STA" << i << ": Throughput=" << throughput
                  << " Mbps, AvgTxPower=" << avgTxPower << " dBm, RX=" << bytes << " bytes\n";
    }
    Simulator::Schedule(Seconds(intervalSeconds), &PrintStats,
                       std::ref(throughputDataset),
                       std::ref(powerDataset),
                       std::ref(stateDatasets),
                       intervalSeconds,
                       std::ref(lastTime));
}

void InstallNodeStats(Ptr<Node> staNode, Ptr<NetDevice> staDev, Ptr<WifiPhy> staPhy, Ptr<Application> sinkApp)
{
    Ptr<NodeStatistics> stat = CreateObject<NodeStatistics>(staNode, staDev, staPhy);
    sinkApp->GetObject<PacketSink>()->TraceConnectWithoutContext(
        "Rx", MakeCallback(&NodeStatistics::PacketReceived, stat));
    g_staStats.push_back(stat);
}

int main(int argc, char *argv[])
{
    uint32_t simulationTime = 100;
    double intervalSeconds = 1.0;
    std::string wifiManager = "ns3::ParfWifiManager";
    uint32_t rtsThreshold = 2347;
    double txPowerStart = 16.0;
    double txPowerEnd = 16.0;
    std::vector<Vector> apPositions = {Vector(0.0, 0.0, 0.0), Vector(30.0, 0.0, 0.0)};
    std::vector<Vector> staPositions = {Vector(0.0, 10.0, 0.0), Vector(30.0, 10.0, 0.0)};

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("wifiManager", "WiFi manager (e.g., ns3::ParfWifiManager, ns3::AarfWifiManager, ...)", wifiManager);
    cmd.AddValue("rtsThreshold", "RTS/CTS Threshold (bytes)", rtsThreshold);
    cmd.AddValue("txPowerStart", "TxPowerStart (dBm)", txPowerStart);
    cmd.AddValue("txPowerEnd", "TxPowerEnd (dBm)", txPowerEnd);
    cmd.AddValue("ap0x", "AP0 X", apPositions[0].x);
    cmd.AddValue("ap0y", "AP0 Y", apPositions[0].y);
    cmd.AddValue("ap1x", "AP1 X", apPositions[1].x);
    cmd.AddValue("ap1y", "AP1 Y", apPositions[1].y);
    cmd.AddValue("sta0x", "STA0 X", staPositions[0].x);
    cmd.AddValue("sta0y", "STA0 Y", staPositions[0].y);
    cmd.AddValue("sta1x", "STA1 X", staPositions[1].x);
    cmd.AddValue("sta1y", "STA1 Y", staPositions[1].y);
    cmd.Parse(argc, argv);

    apPositions[0].x = apPositions[0].x; apPositions[0].y = apPositions[0].y;
    apPositions[1].x = apPositions[1].x; apPositions[1].y = apPositions[1].y;
    staPositions[0].x = staPositions[0].x; staPositions[0].y = staPositions[0].y;
    staPositions[1].x = staPositions[1].x; staPositions[1].y = staPositions[1].y;

    NodeContainer aps, stas;
    aps.Create(2);
    stas.Create(2);

    // Position APs and STAs
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (auto p : apPositions) posAlloc->Add(p);
    for (auto p : staPositions) posAlloc->Add(p);

    NodeContainer allNodes;
    allNodes.Add(aps);
    allNodes.Add(stas);

    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Create WiFi PHY and Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(txPowerStart));
    phy.Set("TxPowerEnd", DoubleValue(txPowerEnd));
    phy.Set("RxGain", DoubleValue(0.0));
    phy.Set("TxGain", DoubleValue(0.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager(wifiManager, "RtsCtsThreshold", UintegerValue(rtsThreshold));

    WifiMacHelper mac;
    NetDeviceContainer apDevices, staDevices;

    std::vector<Ssid> ssids = {Ssid("ssid-ap0"), Ssid("ssid-ap1")};
    for (uint32_t i = 0; i < 2; ++i) {
        // Configure AP
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssids[i]));
        NetDeviceContainer apDev = wifi.Install(phy, mac, aps.Get(i));
        apDevices.Add(apDev);

        // Configure STA
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssids[i]), "ActiveProbing", BooleanValue(false));
        NetDeviceContainer staDev = wifi.Install(phy, mac, stas.Get(i));
        staDevices.Add(staDev);
    }

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> ifaceAps, ifaceStas;

    for (uint32_t i = 0; i < 2; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer apIfc = address.Assign(apDevices.Get(i));
        Ipv4InterfaceContainer staIfc = address.Assign(staDevices.Get(i));
        ifaceAps.push_back(apIfc);
        ifaceStas.push_back(staIfc);
    }

    // PacketSinks and UDP OnOff Apps
    uint16_t basePort = 5000;
    ApplicationContainer sinkApps, onoffApps;
    for (uint32_t i = 0; i < 2; ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), basePort + i));
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
        ApplicationContainer app = sinkHelper.Install(stas.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simulationTime));
        sinkApps.Add(app);
    }

    double appRateMbps = 54.0;
    uint32_t packetSize = 1400;
    for (uint32_t i = 0; i < 2; ++i)
    {
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ifaceStas[i].GetAddress(0), basePort + i));
        DataRateValue rate(DataRate(appRateMbps * 1e6));
        onoff.SetAttribute("DataRate", rate);
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
        ApplicationContainer app = onoff.Install(aps.Get(i));
        onoffApps.Add(app);
    }

    // Setup NodeStatistics
    Config::SetDefault("ns3::WifiPhy::StateHelperInterval", TimeValue(Seconds(intervalSeconds)));
    for (uint32_t i = 0; i < 2; ++i)
    {
        Ptr<NetDevice> dev = staDevices.Get(i);
        Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
        Ptr<WifiPhy> wiphy = wifiDev->GetPhy();
        Ptr<Application> sinkApp = sinkApps.Get(i);
        InstallNodeStats(stas.Get(i), dev, wiphy, sinkApp);
    }

    // Schedule periodic stats printing
    Gnuplot2dDataset throughputDataset("Throughput");
    Gnuplot2dDataset powerDataset("AvgTxPower");
    std::vector<std::string> stateNames = {"CCA_BUSY", "IDLE", "TX", "RX"};
    std::vector<Gnuplot2dDataset> stateDatasets(4);
    for (uint32_t j = 0; j < 4; ++j) stateDatasets[j].SetTitle(stateNames[j]);
    double lastTime = 0.0;
    Simulator::Schedule(Seconds(intervalSeconds), &PrintStats,
                        std::ref(throughputDataset), std::ref(powerDataset), std::ref(stateDatasets),
                        intervalSeconds, std::ref(lastTime));

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    std::cout << "\n------ FlowMonitor Results ------" << std::endl;
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        double duration = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
        double throughput = (flow.second.rxBytes * 8.0) / (duration * 1e6);
        std::cout << "  Throughput: " << throughput << " Mbps\n";
        std::cout << "  Mean Delay: " << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) << " s\n";
        std::cout << "  Mean Jitter: " << (flow.second.jitterSum.GetSeconds() / (flow.second.rxPackets ? flow.second.rxPackets : 1)) << " s\n";
    }

    // Gnuplot setup
    Gnuplot gplotThroughput("sta_throughput.png");
    gplotThroughput.SetTitle("STA Throughput");
    gplotThroughput.SetTerminal("png");
    gplotThroughput.SetLegend("Time [s]", "Throughput [Mbps]");
    gplotThroughput.AddDataset(throughputDataset);
    std::ofstream outPlot1("sta_throughput.plt");
    gplotThroughput.GenerateOutput(outPlot1);
    outPlot1.close();

    Gnuplot gplotPower("sta_avg_txpower.png");
    gplotPower.SetTitle("Avg Transmit Power");
    gplotPower.SetTerminal("png");
    gplotPower.SetLegend("Time [s]", "Tx Power [dBm]");
    gplotPower.AddDataset(powerDataset);
    std::ofstream outPlot2("sta_avg_txpower.plt");
    gplotPower.GenerateOutput(outPlot2);
    outPlot2.close();

    Gnuplot gplotState("sta_wifi_state.png");
    gplotState.SetTitle("WiFi State Times (per sec)");
    gplotState.SetTerminal("png");
    gplotState.SetLegend("Time [s]", "Seconds");
    for (auto &ds : stateDatasets) gplotState.AddDataset(ds);
    std::ofstream outPlot3("sta_wifi_state.plt");
    gplotState.GenerateOutput(outPlot3);
    outPlot3.close();

    Simulator::Destroy();

    return 0;
}