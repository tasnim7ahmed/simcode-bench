#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiSimulation");

class NodeStatistics {
public:
    NodeStatistics(Ptr<Node> node, uint32_t nodeId);

    void PhyStateTrace(std::string context, Time now, WifiPhyState state);
    void RxTrace(std::string context, Ptr<const Packet> packet, const Address &);
    void TxTrace(std::string context, Ptr<const Packet> packet, const Address &);
    void PhyTxBegin(std::string context, Ptr<const Packet> packet);
    void PhyTxEnd(std::string context, Ptr<const Packet> packet);
    void PrintStats(Time interval);

private:
    uint32_t m_nodeId;
    uint64_t m_bytesReceived;
    uint64_t m_totalTxBytes;
    double m_powerAccumulator;
    uint64_t m_powerCount;
    std::map<WifiPhyState, Time> m_stateDurations;
    EventId m_printEvent;
};

NodeStatistics::NodeStatistics(Ptr<Node> node, uint32_t nodeId)
    : m_nodeId(nodeId),
      m_bytesReceived(0),
      m_totalTxBytes(0),
      m_powerAccumulator(0.0),
      m_powerCount(0) {
    for (auto &state : m_stateDurations) {
        state.second = Seconds(0);
    }

    Ptr<WifiNetDevice> wifi = DynamicCast<WifiNetDevice>(node->GetDevice(0));
    if (wifi) {
        wifi->GetPhy()->TraceConnectWithoutContext("PhyRxDrop", MakeCallback(&NodeStatistics::RxTrace, this));
        wifi->GetPhy()->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&NodeStatistics::TxTrace, this));
        wifi->GetPhy()->TraceConnectWithoutContext("State", MakeCallback(&NodeStatistics::PhyStateTrace, this));
        wifi->GetPhy()->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&NodeStatistics::PhyTxBegin, this));
        wifi->GetPhy()->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&NodeStatistics::PhyTxEnd, this));
    }
}

void NodeStatistics::PhyStateTrace(std::string context, Time now, WifiPhyState state) {
    static Time lastChangeTime = Now();
    Time duration = now - lastChangeTime;
    m_stateDurations[state] += duration;
    lastChangeTime = now;
}

void NodeStatistics::RxTrace(std::string context, Ptr<const Packet> packet, const Address &) {
    m_bytesReceived += packet->GetSize();
}

void NodeStatistics::TxTrace(std::string context, Ptr<const Packet> packet, const Address &) {
    m_totalTxBytes += packet->GetSize();
}

void NodeStatistics::PhyTxBegin(std::string context, Ptr<const Packet> packet) {
    // Do nothing
}

void NodeStatistics::PhyTxEnd(std::string context, Ptr<const Packet> packet) {
    // Estimate power used for transmission
    double txPower = 16.0; // Default dBm
    m_powerAccumulator += txPower;
    m_powerCount++;
}

void NodeStatistics::PrintStats(Time interval) {
    NS_LOG_UNCOND("Node " << m_nodeId << " Statistics at " << Now().As(Time::S));
    NS_LOG_UNCOND("Total Bytes Received: " << m_bytesReceived << " bytes");
    NS_LOG_UNCOND("Throughput: " << (double)m_bytesReceived / Now().GetSeconds() * 8 / 1e6 << " Mbps");
    NS_LOG_UNCOND("Average TX Power: " << (m_powerCount > 0 ? m_powerAccumulator / m_powerCount : 0.0) << " dBm");

    for (const auto &entry : m_stateDurations) {
        NS_LOG_UNCOND("Time in " << entry.first << ": " << entry.second.As(Time::S));
    }

    m_bytesReceived = 0;

    m_printEvent = Simulator::Schedule(interval, &NodeStatistics::PrintStats, this, interval);
}

int main(int argc, char *argv[]) {
    std::string managerType = "ParfWifiManager";
    uint32_t rtsThreshold = 65535;
    double txPowerLevel = 16.0;
    double simulationTime = 100.0;
    bool enableFlowMonitor = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("manager", "WiFi rate control manager type", managerType);
    cmd.AddValue("rtsThreshold", "RTS threshold", rtsThreshold);
    cmd.AddValue("txPower", "Transmission power level (dBm)", txPowerLevel);
    cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("enableFlowMonitor", "Enable FlowMonitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
    Config::SetDefault("ns3::WifiRemoteStationManager::DataMode", StringValue("OfdmRate54Mbps"));
    Config::SetDefault("ns3::WifiRemoteStationManager::ControlMode", StringValue("OfdmRate24Mbps"));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate54Mbps"));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue(5));
    Config::SetDefault("ns3::WifiRemoteStationManager::MaxSsrc", UintegerValue(5));
    Config::SetDefault("ns3::YansWifiPhy::TxPowerStart", DoubleValue(txPowerLevel));
    Config::SetDefault("ns3::YansWifiPhy::TxPowerEnd", DoubleValue(txPowerLevel));
    Config::SetDefault("ns3::WifiHelper::DefaultManager", StringValue(managerType));

    NodeContainer apNodes, staNodes;
    apNodes.Create(2);
    staNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::ListPositionAllocator");
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    NetDeviceContainer apDevices[2], staDevices[2];
    Ssid ssid[2];

    ssid[0] = Ssid("network-AP1");
    ssid[1] = Ssid("network-AP2");

    for (int i = 0; i < 2; ++i) {
        WifiMacHelper mac;
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid[i]),
                    "BeaconInterval", TimeValue(Seconds(2.5)));
        apDevices[i] = wifi.Install(phy, mac, apNodes.Get(i));

        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid[i]),
                    "ActiveProbing", BooleanValue(false));
        staDevices[i] = wifi.Install(phy, mac, staNodes.Get(i));
    }

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces[2], staInterfaces[2];
    for (int i = 0; i < 2; ++i) {
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);
        address.NewNetwork();
    }

    UdpServerHelper serverHelper;
    ApplicationContainer serverApps[2];
    for (int i = 0; i < 2; ++i) {
        serverApps[i] = serverHelper.Install(staNodes.Get(i));
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(simulationTime));
    }

    OnOffHelper clientHelper("ns3::UdpSocketFactory", InetSocketAddress());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApps[2];
    for (int i = 0; i < 2; ++i) {
        clientHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(staInterfaces[i].GetAddress(0), 49153)));
        clientApps[i] = clientHelper.Install(apNodes.Get(i));
        clientApps[i].Start(Seconds(1.0));
        clientApps[i].Stop(Seconds(simulationTime - 1));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }

    NodeStatistics stats[4] = {
        NodeStatistics(apNodes.Get(0), 0),
        NodeStatistics(apNodes.Get(1), 1),
        NodeStatistics(staNodes.Get(0), 2),
        NodeStatistics(staNodes.Get(1), 3)
    };

    stats[0].PrintStats(Seconds(1));
    stats[1].PrintStats(Seconds(1));
    stats[2].PrintStats(Seconds(1));
    stats[3].PrintStats(Seconds(1));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    if (enableFlowMonitor) {
        flowmon.SerializeToXmlFile("flowmon.xml", false, false);
    }

    Simulator::Destroy();
    return 0;
}