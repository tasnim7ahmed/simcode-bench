#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWifiNetworkPerformance");

class ThroughputMonitor : public Object {
public:
    static TypeId GetTypeId(void);
    ThroughputMonitor();
    void Start(Ptr<FlowMonitor> monitor, double interval);
private:
    void PrintThroughput();
    Ptr<FlowMonitor> m_monitor;
    EventId m_event;
    double m_interval;
};

TypeId ThroughputMonitor::GetTypeId(void) {
    static TypeId tid = TypeId("ThroughputMonitor")
        .SetParent<Object>()
        .AddConstructor<ThroughputMonitor>();
    return tid;
}

ThroughputMonitor::ThroughputMonitor() {}

void ThroughputMonitor::Start(Ptr<FlowMonitor> monitor, double interval) {
    m_monitor = monitor;
    m_interval = interval;
    m_event = Simulator::Schedule(Seconds(interval), &ThroughputMonitor::PrintThroughput, this);
}

void ThroughputMonitor::PrintThroughput() {
    Time now = Simulator::Now();
    double totalRx = 0;
    std::map<FlowId, FlowMonitor::FlowStats> stats = m_monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        FlowId id = it->first;
        FlowMonitor::FlowStats flow = it->second;
        totalRx += flow.rxBytes * 8.0 / m_interval / 1e6; // Mbps
    }
    NS_LOG_INFO("Time " << now.GetSeconds() << "s: Total Throughput = " << totalRx << " Mbps");
    m_event = Simulator::Schedule(Seconds(m_interval), &ThroughputMonitor::PrintThroughput, this);
}

int main(int argc, char *argv[]) {
    bool enableUdp = true;
    bool enableTcp = true;
    uint32_t numBStations = 2;
    uint32_t numGStations = 2;
    uint32_t numHtStations = 2;
    bool enableProtection = true;
    bool shortPpdu = false;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double interval = 1.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableUdp", "Enable UDP traffic", enableUdp);
    cmd.AddValue("enableTcp", "Enable TCP traffic", enableTcp);
    cmd.AddValue("numBStations", "Number of b stations", numBStations);
    cmd.AddValue("numGStations", "Number of g stations", numGStations);
    cmd.AddValue("numHtStations", "Number of HT stations", numHtStations);
    cmd.AddValue("enableProtection", "Enable ERP protection", enableProtection);
    cmd.AddValue("shortPpdu", "Use Short PPDU format", shortPpdu);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("interval", "Throughput logging interval", interval);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate24Mbps"));

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(numBStations + numGStations + numHtStations);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("mixed-wifi-network");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    if (!enableProtection) {
        wifi.DisableRts();
        Config::SetDefault("ns3::WifiNetDevice::EnableErpOfdmProtection", BooleanValue(false));
    }

    if (shortPpdu) {
        Config::SetDefault("ns3::HeConfiguration::EnableShortPpdu", BooleanValue(true));
    }

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400)),
                "Slot", TimeValue(enableProtection ? MicroSeconds(20) : MicroSeconds(9)));
    apDevice = wifi.Install(phy, mac, apNode);

    NetDeviceContainer staDevices;
    uint32_t idx = 0;

    // b stations
    wifi.SetStandard(WIFI_STANDARD_80211b_2_4GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate1Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));
    for (; idx < numBStations; ++idx) {
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices.Add(wifi.Install(phy, mac, staNodes.Get(idx)));
    }

    // g stations
    wifi.SetStandard(WIFI_STANDARD_80211g_2_4GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("ErpOfdmRate54Mbps"), "ControlMode", StringValue("ErpOfdmRate6Mbps"));
    for (; idx < numBStations + numGStations; ++idx) {
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices.Add(wifi.Install(phy, mac, staNodes.Get(idx)));
    }

    // HT stations
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHz);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    for (; idx < numBStations + numGStations + numHtStations; ++idx) {
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices.Add(wifi.Install(phy, mac, staNodes.Get(idx)));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    ApplicationContainer udpApps, tcpApps;
    uint16_t port = 9;

    for (uint32_t i = 0; i < staInterfaces.GetN(); ++i) {
        if (enableUdp) {
            OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
            onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
            onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

            ApplicationContainer app = onoff.Install(staNodes.Get(i));
            app.Start(Seconds(1.0));
            app.Stop(Seconds(simulationTime - 0.1));
            udpApps.Add(app);
        }

        if (enableTcp) {
            BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port + 1));
            bulkSend.SetAttribute("MaxBytes", UintegerValue(0));

            ApplicationContainer app = bulkSend.Install(staNodes.Get(i));
            app.Start(Seconds(1.0));
            app.Stop(Seconds(simulationTime - 0.1));
            tcpApps.Add(app);

            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
            ApplicationContainer sinkApp = sink.Install(apNode.Get(0));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(simulationTime));
        }
    }

    PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSink.Install(apNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    ThroughputMonitor tm;
    tm.Start(monitor, interval);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalThroughput = 0;
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowId id = iter->first;
        FlowMonitor::FlowStats flow = iter->second;
        totalThroughput += flow.rxBytes * 8.0 / simulationTime / 1e6;
    }

    NS_LOG_UNCOND("Total Throughput: " << totalThroughput << " Mbps");
    return 0;
}