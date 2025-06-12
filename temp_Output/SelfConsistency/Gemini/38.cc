#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

// Custom data structure to store WiFi state times
struct WifiStateTimes {
    Time ccaBusy;
    Time idle;
    Time tx;
    Time rx;

    WifiStateTimes() : ccaBusy(Seconds(0)), idle(Seconds(0)), tx(Seconds(0)), rx(Seconds(0)) {}
};

// NodeStatistics class to track and print statistics
class NodeStatistics {
public:
    NodeStatistics(NodeContainer nodes, uint32_t staId, Ipv4Address sinkAddress, uint16_t sinkPort)
        : m_nodes(nodes), m_staId(staId), m_sinkAddress(sinkAddress), m_sinkPort(sinkPort)
    {
        m_wifiStateTimes = WifiStateTimes();
    }

    void Initialize() {
        // Trace sources for WiFi PHY states
        std::stringstream ss;
        ss << "/NodeList/" << m_staId << "/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx";
        m_monitorSnifferRxTraceSource = ss.str();

        ss.str("");
        ss << "/NodeList/" << m_staId << "/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx";
        m_monitorSnifferTxTraceSource = ss.str();

        ss.str("");
        ss << "/NodeList/" << m_staId << "/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State";
        m_phyStateTraceSource = ss.str();

        Config::Connect(m_monitorSnifferRxTraceSource, MakeCallback(&NodeStatistics::MonitorSnifferRx, this));
        Config::Connect(m_monitorSnifferTxTraceSource, MakeCallback(&NodeStatistics::MonitorSnifferTx, this));
        Config::Connect(m_phyStateTraceSource, MakeCallback(&NodeStatistics::PhyStateChange, this));

        m_totalBytesReceived = 0;
        m_totalTxPower = 0;
        m_numTxPackets = 0;
    }

    void Reset() {
        m_wifiStateTimes = WifiStateTimes();
        m_totalBytesReceived = 0;
        m_totalTxPower = 0;
        m_numTxPackets = 0;
    }

    void SetWifiManager(std::string wifiManagerType)
    {
      m_wifiManagerType = wifiManagerType;
    }

    // Callback for received packets
    void MonitorSnifferRx(Ptr<const Packet> packet, uint16_t channelFreq, WifiBand wifiBand, uint16_t bandwidth, WifiPreamble preamble) {
        m_totalBytesReceived += packet->GetSize();
    }

    // Callback for transmitted packets
    void MonitorSnifferTx(Ptr<const Packet> packet, uint16_t channelFreq, WifiBand wifiBand, uint16_t bandwidth, WifiPreamble preamble, uint8_t txPowerLevel) {
        m_totalTxPower += txPowerLevel;
        m_numTxPackets++;
    }

    // Callback for PHY state changes
    void PhyStateChange(std::string oldValue, std::string newValue) {
        Time now = Simulator::Now();
        if (newValue == "CCA_BUSY") {
            m_wifiStateTimes.ccaBusy += now - m_lastStateChange;
        } else if (newValue == "IDLE") {
            m_wifiStateTimes.idle += now - m_lastStateChange;
        } else if (newValue == "TX") {
            m_wifiStateTimes.tx += now - m_lastStateChange;
        } else if (newValue == "RX") {
            m_wifiStateTimes.rx += now - m_lastStateChange;
        }
        m_lastStateChange = now;
    }

    // Calculate and print statistics
    void PrintStatistics(double interval) {
        double throughput = (m_totalBytesReceived * 8.0) / (interval * 1000000.0); // Mbps
        double avgTxPower = (m_numTxPackets > 0) ? (m_totalTxPower / m_numTxPackets) : 0;

        std::cout << "STA " << m_staId << " Statistics at " << Simulator::Now().Seconds() << "s:" << std::endl;
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " Mbps" << std::endl;
        std::cout << "  Avg Tx Power: " << std::fixed << std::setprecision(2) << avgTxPower << " dBm" << std::endl;
        std::cout << "  CCA Busy Time: " << m_wifiStateTimes.ccaBusy.GetSeconds() << " s" << std::endl;
        std::cout << "  Idle Time: " << m_wifiStateTimes.idle.GetSeconds() << " s" << std::endl;
        std::cout << "  Tx Time: " << m_wifiStateTimes.tx.GetSeconds() << " s" << std::endl;
        std::cout << "  Rx Time: " << m_wifiStateTimes.rx.GetSeconds() << " s" << std::endl;
    }

    double GetThroughput(double interval) const {
      return (m_totalBytesReceived * 8.0) / (interval * 1000000.0);
    }

private:
    NodeContainer m_nodes;
    uint32_t m_staId;
    Ipv4Address m_sinkAddress;
    uint16_t m_sinkPort;
    WifiStateTimes m_wifiStateTimes;
    std::string m_monitorSnifferRxTraceSource;
    std::string m_monitorSnifferTxTraceSource;
    std::string m_phyStateTraceSource;
    Time m_lastStateChange;
    uint64_t m_totalBytesReceived;
    double m_totalTxPower;
    uint32_t m_numTxPackets;
    std::string m_wifiManagerType;
};

int main(int argc, char *argv[]) {
    bool enablePcap = false;
    std::string wifiManagerType = "ns3::ParfWifiManager";
    uint32_t rtsCtsThreshold = 2347;
    double txPowerLevel = 16.0206; // dBm, equivalent to 40 mW
    double simulationTime = 100.0; // Seconds
    double interval = 1.0;

    // Command-line arguments
    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("wifiManagerType", "Set WiFi Manager Type", wifiManagerType);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold", rtsCtsThreshold);
    cmd.AddValue("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    // Create nodes: 2 APs and 2 STAs
    NodeContainer apNodes, staNodes;
    apNodes.Create(2);
    staNodes.Create(2);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::ListPositionAllocator");
    Ptr<ListPositionAllocator> positionAlloc = DynamicCast<ListPositionAllocator>(mobility.GetPositionAllocator());

    // Command line arguments for node positions

    positionAlloc->Add(Vector(10.0, 10.0, 0.0));  // AP0
    positionAlloc->Add(Vector(50.0, 10.0, 0.0));  // AP1
    positionAlloc->Add(Vector(10.0, 20.0, 0.0));  // STA0
    positionAlloc->Add(Vector(50.0, 20.0, 0.0));  // STA1

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // Configure WiFi PHY and channel
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure WiFi MAC
    WifiMacHelper wifiMac;
    Ssid ssid1 = Ssid("ns3-ssid-1");
    Ssid ssid2 = Ssid("ns3-ssid-2");

    // Configure STA devices
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid1),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices1 = wifi.Install(wifiPhy, wifiMac, staNodes.Get(0));

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid2),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(wifiPhy, wifiMac, staNodes.Get(1));


    // Configure AP devices
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid1),
                    "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer apDevices1 = wifi.Install(wifiPhy, wifiMac, apNodes.Get(0));

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid2),
                    "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer apDevices2 = wifi.Install(wifiPhy, wifiMac, apNodes.Get(1));


    // Set transmission power and RTS/CTS threshold
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerLevel));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerLevel));
    Config::SetGlobalDefaults("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsCtsThreshold));

    // Set WifiManager
    Config::SetDefault ("ns3::WifiNetDevice::ActiveManager", StringValue (wifiManagerType));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(apNodes);
    internet.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);


    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure CBR traffic from AP to STA
    uint16_t port1 = 9;
    uint16_t port2 = 10;

    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(staInterfaces1.GetAddress(0), port1)));
    onoff1.SetConstantRate(DataRate("54Mbps"));
    ApplicationContainer clientApps1 = onoff1.Install(apNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simulationTime));

    OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(staInterfaces2.GetAddress(0), port2)));
    onoff2.SetConstantRate(DataRate("54Mbps"));
    ApplicationContainer clientApps2 = onoff2.Install(apNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(simulationTime));



    // Install packet sink on STAs
    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApps1 = sink1.Install(staNodes.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(simulationTime));

    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApps2 = sink2.Install(staNodes.Get(1));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(simulationTime));


    // Create NodeStatistics objects
    NodeStatistics stats1(staNodes, 0, staInterfaces1.GetAddress(0), port1);
    stats1.Initialize();
    stats1.SetWifiManager(wifiManagerType);

    NodeStatistics stats2(staNodes, 1, staInterfaces2.GetAddress(0), port2);
    stats2.Initialize();
    stats2.SetWifiManager(wifiManagerType);

    // Schedule statistics printing
    Simulator::Schedule(Seconds(0.0), [&]() { stats1.Reset(); stats2.Reset(); }); // Reset at the beginning
    for (double t = interval; t <= simulationTime; t += interval) {
        Simulator::Schedule(Seconds(t), [&, t]() {
            stats1.PrintStatistics(interval);
            stats2.PrintStatistics(interval);
        });
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable PCAP tracing
    if (enablePcap) {
        wifiPhy.EnablePcapAll("wifi-example");
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.Seconds() - i->second.timeFirstTxPacket.Seconds()) / 1000000 << " Mbps\n";
        std::cout << "  Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
        std::cout << "  Jitter: " << i->second.jitterSum.GetSeconds() / (i->second.rxPackets - 1) << " s\n";
    }

    // Gnuplot output
    std::string plotFileName = "wifi-example.plt";
    std::string graphicsFileName = "wifi-example.png";
    std::string plotTitle = "WiFi Network Performance";

    Gnuplot gnuplot(plotFileName);
    gnuplot.SetTitle(plotTitle);
    gnuplot.SetTerminal("png");
    gnuplot.SetOutput(graphicsFileName);
    gnuplot.SetLegend("Time (s)", "Throughput (Mbps)");

    Gnuplot2dDataset dataset1;
    dataset1.SetTitle("STA 0 Throughput");
    dataset1.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    Gnuplot2dDataset dataset2;
    dataset2.SetTitle("STA 1 Throughput");
    dataset2.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    std::vector<double> throughput_sta0;
    std::vector<double> throughput_sta1;

    for (double t = interval; t <= simulationTime; t += interval) {
        throughput_sta0.push_back(0.0);
        throughput_sta1.push_back(0.0);
    }

    Simulator::Destroy();
    return 0;
}