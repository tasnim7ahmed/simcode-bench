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
    NodeStatistics(Ptr<Node> node, std::string nodeName);

    void PhyTxBegin(Ptr<const Packet>);
    void PhyTxEnd(Ptr<const Packet>);
    void PhyRxBegin(Ptr<const Packet>);
    void PhyRxEnd(Ptr<const Packet>, bool);
    void StateChange(Time start, Time end, WifiPhyState state);
    void UpdatePower(double power);
    void Report();

private:
    Ptr<Node> m_node;
    std::string m_nodeName;
    uint64_t m_bytesReceived;
    double m_totalPower;
    uint64_t m_powerCount;
    Time m_lastReportTime;
    std::map<WifiPhyState, Time> m_stateDurations;
    WifiPhyState m_currentState;
    Time m_stateStartTime;
};

NodeStatistics::NodeStatistics(Ptr<Node> node, std::string nodeName)
    : m_node(node),
      m_nodeName(nodeName),
      m_bytesReceived(0),
      m_totalPower(0.0),
      m_powerCount(0),
      m_lastReportTime(Seconds(0)),
      m_currentState(WifiPhyState::IDLE) {
    for (auto state : {WifiPhyState::CCA_BUSY, WifiPhyState::IDLE, WifiPhyState::TX, WifiPhyState::RX}) {
        m_stateDurations[state] = Seconds(0);
    }

    Config::ConnectWithoutContext("/NodeList/" + std::to_string(m_node->GetId()) + "/ApplicationList/*/$ns3::PacketSink/Rx",
                                  MakeCallback(&NodeStatistics::PhyRxEnd, this));
}

void NodeStatistics::PhyTxBegin(Ptr<const Packet> packet) {
    m_currentState = WifiPhyState::TX;
    m_stateStartTime = Simulator::Now();
}

void NodeStatistics::PhyTxEnd(Ptr<const Packet> packet) {
    Time duration = Simulator::Now() - m_stateStartTime;
    m_stateDurations[m_currentState] += duration;
    m_currentState = WifiPhyState::IDLE;
    m_stateStartTime = Simulator::Now();
}

void NodeStatistics::PhyRxBegin(Ptr<const Packet> packet) {
    m_currentState = WifiPhyState::RX;
    m_stateStartTime = Simulator::Now();
}

void NodeStatistics::PhyRxEnd(Ptr<const Packet> packet, bool inError) {
    Time duration = Simulator::Now() - m_stateStartTime;
    m_stateDurations[m_currentState] += duration;
    m_bytesReceived += packet->GetSize();
    m_currentState = WifiPhyState::IDLE;
    m_stateStartTime = Simulator::Now();

    if ((Simulator::Now() - m_lastReportTime) >= Seconds(1)) {
        Report();
        m_lastReportTime = Simulator::Now();
    }
}

void NodeStatistics::StateChange(Time start, Time end, WifiPhyState state) {
    m_stateDurations[m_currentState] += start - m_stateStartTime;
    m_currentState = state;
    m_stateStartTime = start;
}

void NodeStatistics::UpdatePower(double power) {
    m_totalPower += power;
    m_powerCount++;
}

void NodeStatistics::Report() {
    double throughput = static_cast<double>(m_bytesReceived) * 8 / 1e6; // Mbps
    m_bytesReceived = 0;

    double avgPower = m_powerCount > 0 ? m_totalPower / m_powerCount : 0;
    m_totalPower = 0.0;
    m_powerCount = 0;

    std::cout << Simulator::Now().GetSeconds() << "s " << m_nodeName << ": "
              << "Throughput: " << throughput << " Mbps, "
              << "Avg TX Power: " << avgPower << " dBm, "
              << "CCA Busy: " << m_stateDurations[WifiPhyState::CCA_BUSY].GetSeconds() << "s, "
              << "Idle: " << m_stateDurations[WifiPhyState::IDLE].GetSeconds() << "s, "
              << "TX: " << m_stateDurations[WifiPhyState::TX].GetSeconds() << "s, "
              << "RX: " << m_stateDurations[WifiPhyState::RX].GetSeconds() << "s" << std::endl;

    for (auto& pair : m_stateDurations) {
        pair.second = Seconds(0);
    }
}

static void GenerateGnuplotFiles(std::vector<double>& xValues, std::vector<double>& yValues,
                                std::string baseName, std::string title, std::string yAxisLabel) {
    std::ostringstream filename;
    filename << baseName << "-data.plt";
    Gnuplot plot(baseName + ".png");
    plot.SetTitle(title);
    plot.SetLegend("Time (s)", yAxisLabel.c_str());

    Gnuplot2dDataset dataset;
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
    dataset.SetTitle("");

    for (size_t i = 0; i < xValues.size(); ++i) {
        dataset.Add(xValues[i], yValues[i]);
    }

    plot.AddDataset(dataset);
    std::ofstream plotFile(filename.str().c_str());
    plot.GenerateOutput(plotFile);
    plotFile.close();
}

int main(int argc, char* argv[]) {
    std::string wifiManager = "ParfWifiManager";
    uint32_t rtsThreshold = 65535; // default no RTS/CTS
    double txPowerLevel = 16;     // dBm
    double simulationTime = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("wifiManager", "Type of WiFi manager to use", wifiManager);
    cmd.AddValue("rtsThreshold", "RTS threshold", rtsThreshold);
    cmd.AddValue("txPowerLevel", "Transmit power level (dBm)", txPowerLevel);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(2);

    // Position allocation
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::ListPositionAllocator");

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    // Set positions manually (for example)
    Config::Set("/NodeList/0/DeviceList/0/MobilityModel/PositionAllocator/X", DoubleValue(0.0));
    Config::Set("/NodeList/0/DeviceList/0/MobilityModel/PositionAllocator/Y", DoubleValue(0.0));

    Config::Set("/NodeList/1/DeviceList/0/MobilityModel/PositionAllocator/X", DoubleValue(10.0));
    Config::Set("/NodeList/1/DeviceList/0/MobilityModel/PositionAllocator/Y", DoubleValue(0.0));

    Config::Set("/NodeList/2/DeviceList/0/MobilityModel/PositionAllocator/X", DoubleValue(5.0));
    Config::Set("/NodeList/2/DeviceList/0/MobilityModel/PositionAllocator/Y", DoubleValue(10.0));

    Config::Set("/NodeList/3/DeviceList/0/MobilityModel/PositionAllocator/X", DoubleValue(15.0));
    Config::Set("/NodeList/3/DeviceList/0/MobilityModel/PositionAllocator/Y", DoubleValue(10.0));

    // Setup channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configure WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + wifiManager);

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("network-1");
    Ssid ssid2 = Ssid("network-2");

    // Install APs
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    // Install STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes.Get(0));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes.Get(1));

    // Configure RTS/CTS
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold",
                UintegerValue(rtsThreshold));

    // Configure transmit power
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerLevel", DoubleValue(txPowerLevel));

    // Internet stack
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

    // Applications
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));

    ApplicationContainer sinkApps1 = sinkHelper.Install(staNodes.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(simulationTime));

    ApplicationContainer sinkApps2 = sinkHelper.Install(staNodes.Get(1));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces1.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer sourceApps1 = onoff.Install(apNodes.Get(0));
    sourceApps1.Start(Seconds(0.0));
    sourceApps1.Stop(Seconds(simulationTime));

    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(staInterfaces2.GetAddress(0), port)));
    ApplicationContainer sourceApps2 = onoff.Install(apNodes.Get(1));
    sourceApps2.Start(Seconds(0.0));
    sourceApps2.Stop(Seconds(simulationTime));

    // Statistics tracking
    NodeStatistics statsAp1(apNodes.Get(0), "AP1");
    NodeStatistics statsSta1(staNodes.Get(0), "STA1");
    NodeStatistics statsAp2(apNodes.Get(1), "AP2");
    NodeStatistics statsSta2(staNodes.Get(1), "STA2");

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback(&NodeStatistics::PhyTxBegin, &statsAp1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback(&NodeStatistics::PhyTxBegin, &statsSta1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback(&NodeStatistics::PhyTxBegin, &statsAp2));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback(&NodeStatistics::PhyTxBegin, &statsSta2));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback(&NodeStatistics::PhyRxBegin, &statsAp1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback(&NodeStatistics::PhyRxBegin, &statsSta1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback(&NodeStatistics::PhyRxBegin, &statsAp2));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback(&NodeStatistics::PhyRxBegin, &statsSta2));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/StateChanged", MakeCallback(&NodeStatistics::StateChange, &statsAp1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/StateChanged", MakeCallback(&NodeStatistics::StateChange, &statsSta1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/StateChanged", MakeCallback(&NodeStatistics::StateChange, &statsAp2));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/StateChanged", MakeCallback(&NodeStatistics::StateChange, &statsSta2));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPower", MakeCallback(&NodeStatistics::UpdatePower, &statsAp1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPower", MakeCallback(&NodeStatistics::UpdatePower, &statsSta1));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPower", MakeCallback(&NodeStatistics::UpdatePower, &statsAp2));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPower", MakeCallback(&NodeStatistics::UpdatePower, &statsSta2));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress
                  << " Protocol " << t.protocol << "\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s\n";
        std::cout << "  Mean Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}