#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiNetworkSimulation");

class NodeStatistics {
public:
    NodeStatistics(Ptr<Node> node, uint32_t nodeId);
    void PhyStateTrace(std::string context, Time now, WifiPhyState state);
    void RxTrace(Ptr<const Packet> packet, const Address &from);
    void UpdateTxPower(double txPower);
    void PrintStats(Time interval);
    void CalculateThroughput();

private:
    uint32_t m_nodeId;
    std::map<WifiPhyState, Time> m_stateDurations;
    uint64_t m_totalRxBytes;
    double m_avgTxPower;
    uint64_t m_prevRxBytes;
    Time m_lastUpdate;
};

NodeStatistics::NodeStatistics(Ptr<Node> node, uint32_t nodeId)
    : m_nodeId(nodeId),
      m_totalRxBytes(0),
      m_avgTxPower(0.0),
      m_prevRxBytes(0),
      m_lastUpdate(Seconds(0))
{
    m_stateDurations[WifiPhyState::IDLE] = Seconds(0);
    m_stateDurations[WifiPhyState::CCA_BUSY] = Seconds(0);
    m_stateDurations[WifiPhyState::TX] = Seconds(0);
    m_stateDurations[WifiPhyState::RX] = Seconds(0);

    node->GetObject<PhyLayer>()->
        TraceConnectWithoutContext("PhyState", MakeCallback(&NodeStatistics::PhyStateTrace, this));
    DynamicCast<PacketSink>(node->GetApplication(0))->
        GetReceiveCallback().Connect(MakeCallback(&NodeStatistics::RxTrace, this));

    Simulator::ScheduleNow(&NodeStatistics::PrintStats, this, interval);
}

void NodeStatistics::PhyStateTrace(std::string context, Time now, WifiPhyState state) {
    Time duration = now - m_lastUpdate;
    if (m_lastUpdate.GetSeconds() > 0 && state != WifiPhyState::SWITCHING) {
        m_stateDurations[m_prevState] += duration;
    }
    m_prevState = state;
    m_lastUpdate = now;
}

void NodeStatistics::RxTrace(Ptr<const Packet> packet, const Address &from) {
    m_totalRxBytes += packet->GetSize();
}

void NodeStatistics::UpdateTxPower(double txPower) {
    m_avgTxPower = (m_avgTxPower * (m_totalRxBytes / (double)(m_totalRxBytes + 1))) +
                   (txPower * (1.0 / (m_totalRxBytes + 1)));
}

void NodeStatistics::PrintStats(Time interval) {
    CalculateThroughput();
    NS_LOG_UNCOND("Node " << m_nodeId <<
                  " Throughput: " << m_currentThroughput << " Mbps" <<
                  ", Avg TX Power: " << m_avgTxPower << " dBm" <<
                  ", CCA Busy: " << m_stateDurations[WifiPhyState::CCA_BUSY].GetSeconds() << "s" <<
                  ", IDLE: " << m_stateDurations[WifiPhyState::IDLE].GetSeconds() << "s" <<
                  ", TX: " << m_stateDurations[WifiPhyState::TX].GetSeconds() << "s" <<
                  ", RX: " << m_stateDurations[WifiPhyState::RX].GetSeconds() << "s");
    Simulator::Schedule(interval, &NodeStatistics::PrintStats, this, interval);
}

void NodeStatistics::CalculateThroughput() {
    double intervalSec = (Simulator::Now() - m_lastThroughputTime).GetSeconds();
    if (intervalSec > 0) {
        m_currentThroughput = (m_totalRxBytes - m_prevRxBytes) * 8.0 / (intervalSec * 1e6);
        m_prevRxBytes = m_totalRxBytes;
        m_lastThroughputTime = Simulator::Now();
    }
}

int main(int argc, char *argv[]) {
    std::string wifiManagerType = "ParfWifiManager";
    uint32_t rtsThreshold = 2346;
    int64_t txPowerLevel = 16; // dBm
    double simulationTime = 100.0;
    bool enableFlowMonitor = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("wifiManager", "WiFi rate control manager type", wifiManagerType);
    cmd.AddValue("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
    cmd.AddValue("txPower", "Transmit power level (dBm)", txPowerLevel);
    cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
    Config::SetDefault("ns3::WifiPhy::TxPowerStart", DoubleValue(txPowerLevel));
    Config::SetDefault("ns3::WifiPhy::TxPowerEnd", DoubleValue(txPowerLevel));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate54Mbps"));
    Config::SetDefault("ns3::WifiRemoteStationManager::DefaultTxPowerLevel", UintegerValue(0));
    Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue("OfdmRate54Mbps"));
    Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue("OfdmRate54Mbps"));
    Config::SetDefault("ns3::WifiRemoteStationManager::WifiManager", StringValue(wifiManagerType));

    NodeContainer apNodes, staNodes;
    apNodes.Create(2);
    staNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate54Mbps"),
                                 "ControlMode", StringValue("OfdmRate54Mbps"));

    Ssid ssid1 = Ssid("network-AP1");
    Ssid ssid2 = Ssid("network-AP2");

    WifiMacHelper mac;
    NetDeviceContainer apDevices, staDevices;

    // AP1 and STA1
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(0)));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1), "ActiveProbing", BooleanValue(false));
    staDevices.Add(wifi.Install(phy, mac, staNodes.Get(0)));

    // AP2 and STA2
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices.Add(wifi.Install(phy, mac, apNodes.Get(1)));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2), "ActiveProbing", BooleanValue(false));
    staDevices.Add(wifi.Install(phy, mac, staNodes.Get(1)));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::ListPositionAllocator");
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces, staInterfaces;
    apInterfaces.Add(address.Assign(apDevices.Get(0)));
    staInterfaces.Add(address.Assign(staDevices.Get(0)));
    address.NewNetwork();
    apInterfaces.Add(address.Assign(apDevices.Get(1)));
    staInterfaces.Add(address.Assign(staDevices.Get(1)));

    uint16_t port = 9;

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        sinkApps.Add(sinkHelper.Install(staNodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1472)); // UDP payload size

    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < apNodes.GetN(); ++i) {
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(staInterfaces.GetAddress(i), port)));
        sourceApps.Add(onoff.Install(apNodes.Get(i)));
    }

    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime - 2));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }

    Gnuplot throughputPlot = Gnuplot("throughput.png");
    Gnuplot avgPowerPlot = Gnuplot("avg_power.png");
    Gnuplot statePlot = Gnuplot("state_times.png");

    std::vector<Ptr<NodeStatistics>> stats;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        stats.push_back(CreateObject<NodeStatistics>(staNodes.Get(i), i));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    if (enableFlowMonitor) {
        flowmon.SerializeToXmlFile("wifi-flow.xml", false, false);
    }

    Simulator::Destroy();
    return 0;
}