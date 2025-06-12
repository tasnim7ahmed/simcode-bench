#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiNetworkSimulation");

class NodeStatistics {
public:
    NodeStatistics(Ptr<Node> node);
    void PhyStateTrace(std::string context, Time now, WifiPhyState state);
    void PacketRxTrace(Ptr<const Packet> packet, double startSnr, double endSnr);
    void Report();
    void CalculateStats();

private:
    Ptr<Node> m_node;
    std::map<WifiPhyState, Time> m_stateDurations;
    uint64_t m_bytesReceived;
    Time m_lastReportTime;
    double m_avgTxPower;
    uint32_t m_powerCount;
    uint32_t m_txCount;
    double m_totalThroughput;
};

NodeStatistics::NodeStatistics(Ptr<Node> node)
    : m_node(node),
      m_bytesReceived(0),
      m_lastReportTime(Seconds(0)),
      m_avgTxPower(0.0),
      m_powerCount(0),
      m_txCount(0),
      m_totalThroughput(0.0)
{
    for (auto& state : WifiPhyStateHelper::GetStates())
    {
        m_stateDurations[state] = Seconds(0);
    }

    Config::ConnectWithoutContext("/NodeList/" + std::to_string(node->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Phy/State",
                                  MakeCallback(&NodeStatistics::PhyStateTrace, this));
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(node->GetId()) + "/DeviceList/*/$ns3::WifiNetDevice/Phy/RxOk",
                                  MakeCallback(&NodeStatistics::PacketRxTrace, this));
}

void NodeStatistics::PhyStateTrace(std::string context, Time now, WifiPhyState state)
{
    static Time lastStateChange = now;

    for (auto& entry : m_stateDurations)
    {
        if (entry.first == state)
            continue;
        entry.second += now - lastStateChange;
    }
    lastStateChange = now;
}

void NodeStatistics::PacketRxTrace(Ptr<const Packet> packet, double startSnr, double endSnr)
{
    m_bytesReceived += packet->GetSize();
}

void NodeStatistics::CalculateStats()
{
    Time now = Simulator::Now();
    Time interval = now - m_lastReportTime;

    double intervalSeconds = interval.GetSeconds();
    m_totalThroughput = (m_bytesReceived * 8.0) / intervalSeconds / 1e6; // Mbps

    m_lastReportTime = now;
}

void NodeStatistics::Report()
{
    CalculateStats();
    NS_LOG_UNCOND("Node " << m_node->GetId() << " Throughput: " << m_totalThroughput << " Mbps");
    NS_LOG_UNCOND("Bytes received: " << m_bytesReceived);
    NS_LOG_UNCOND("Avg TX Power: " << m_avgTxPower / m_powerCount);
    NS_LOG_UNCOND("State durations:");
    for (auto& entry : m_stateDurations)
    {
        NS_LOG_UNCOND("  " << entry.first << ": " << entry.second.As(Time::S));
    }
}

static void PlotResults(NodeContainer aps, NodeContainer stas)
{
    Gnuplot plot("wifi_simulation_results.png");
    plot.SetTitle("WiFi Simulation Results");
    plot.SetTerminal("png size 1024,768 enhanced font 'Verdana,10'");
    plot.SetLegend("Time (seconds)", "Value");

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Throughput");
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    for (size_t i = 0; i < stas.GetN(); ++i)
    {
        auto stats = stas.Get(i)->GetObject<NodeStatistics>();
        dataset.Add(Simulator::Now().GetSeconds(), stats->m_totalThroughput);
    }

    plot.AddDataset(dataset);

    std::ofstream plotFile("wifi_simulation_results.plt");
    plot.GenerateOutput(plotFile);
}

int main(int argc, char* argv[])
{
    std::string wifiManagerType = "ns3::ParfWifiManager";
    uint32_t rtsCtsThreshold = 2346;
    int64_t txPowerLevel0 = 16; // dBm
    int64_t txPowerLevel1 = 16; // dBm
    double simulationTime = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("wifiManager", "WiFi Manager Type", wifiManagerType);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS Threshold", rtsCtsThreshold);
    cmd.AddValue("txPowerLevel0", "Transmission Power Level AP0", txPowerLevel0);
    cmd.AddValue("txPowerLevel1", "Transmission Power Level AP1", txPowerLevel1);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(2);
    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + wifiManagerType);

    WifiMacHelper mac;
    Ssid ssid0 = Ssid("network-0");
    Ssid ssid1 = Ssid("network-1");

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices0;
    NetDeviceContainer staDevices1;

    // Configure AP 0 and STA 0
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid0),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    phy.Set("TxPowerStart", DoubleValue(txPowerLevel0));
    phy.Set("TxPowerEnd", DoubleValue(txPowerLevel0));
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("CcaMode1Threshold", DoubleValue(-79));
    phy.Set("RtsCtsThreshold", UintegerValue(rtsCtsThreshold));

    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid0),
                "ActiveProbing", BooleanValue(false));
    staDevices0 = wifi.Install(phy, mac, staNodes.Get(0));

    // Configure AP 1 and STA 1
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    phy.Set("TxPowerStart", DoubleValue(txPowerLevel1));
    phy.Set("TxPowerEnd", DoubleValue(txPowerLevel1));

    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy, mac, staNodes.Get(1));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::ListPositionAllocator");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces;
    Ipv4InterfaceContainer staInterfaces0;
    Ipv4InterfaceContainer staInterfaces1;

    apInterfaces.Add(address.Assign(apDevices.Get(0)));
    staInterfaces0.Add(address.Assign(staDevices0));
    address.NewNetwork();
    apInterfaces.Add(address.Assign(apDevices.Get(1)));
    staInterfaces1.Add(address.Assign(staDevices1));

    // Applications
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));

    ApplicationContainer sinkApps0 = sink.Install(staNodes.Get(0));
    sinkApps0.Start(Seconds(0.0));
    sinkApps0.Stop(Seconds(simulationTime));

    ApplicationContainer sinkApps1 = sink.Install(staNodes.Get(1));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));

    AddressValue remoteAddress0(InetSocketAddress(staInterfaces0.GetAddress(0), port));
    onoff.SetAttribute("Remote", remoteAddress0);
    ApplicationContainer sourceApps0 = onoff.Install(apNodes.Get(0));
    sourceApps0.Start(Seconds(1.0));
    sourceApps0.Stop(Seconds(simulationTime));

    AddressValue remoteAddress1(InetSocketAddress(staInterfaces1.GetAddress(0), port));
    onoff.SetAttribute("Remote", remoteAddress1);
    ApplicationContainer sourceApps1 = onoff.Install(apNodes.Get(1));
    sourceApps1.Start(Seconds(1.0));
    sourceApps1.Stop(Seconds(simulationTime));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Statistics
    NodeContainer allNodes;
    allNodes.Add(apNodes);
    allNodes.Add(staNodes);

    for (auto it = allNodes.Begin(); it != allNodes.End(); ++it)
    {
        (*it)->AggregateObject(CreateObject<NodeStatistics>(*it));
    }

    // Schedule periodic reporting
    Simulator::Schedule(Seconds(1.0), [apNodes, staNodes]()
    {
        for (uint32_t i = 0; i < staNodes.GetN(); ++i)
        {
            staNodes.Get(i)->GetObject<NodeStatistics>()->Report();
        }
        PlotResults(apNodes, staNodes);
        Simulator::Schedule(Seconds(1.0), &PlotResults, apNodes, staNodes);
    });

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1e6 << " Mbps\n";
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s\n";
        std::cout << "  Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}