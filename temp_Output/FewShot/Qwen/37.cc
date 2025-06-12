#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

class ThroughputTracker {
public:
    ThroughputTracker(std::string powerFile, std::string rateFile, std::string throughputFile)
        : m_powerPlot(powerFile), m_ratePlot(rateFile), m_throughputPlot(throughputFile) {
        m_powerPlot.SetTitle("Transmit Power vs Distance");
        m_powerPlot.SetXTitle("Distance (meters)");
        m_powerPlot.SetYTitle("Tx Power (dBm)");

        m_ratePlot.SetTitle("Data Rate vs Distance");
        m_ratePlot.SetXTitle("Distance (meters)");
        m_ratePlot.SetYTitle("Data Rate (Mbps)");

        m_throughputPlot.SetTitle("Throughput vs Distance");
        m_throughputPlot.SetXTitle("Distance (meters)");
        m_throughputPlot.SetYTitle("Throughput (Mbps)");
    }

    void LogTxParams(double distance, double power, double rate) {
        m_powerPlot.AddData(distance, power);
        m_ratePlot.AddData(distance, rate);
    }

    void LogThroughput(double distance, double throughput) {
        m_throughputPlot.AddData(distance, throughput);
    }

    void Write() {
        Gnuplot2dDataset powerDataset = m_powerPlot.GetDatasetList().Get(0);
        Gnuplot2dDataset rateDataset = m_ratePlot.GetDatasetList().Get(0);
        Gnuplot2dDataset throughputDataset = m_throughputPlot.GetDatasetList().Get(0);

        GnuplotHelper::Write2dData("tx-power.data", powerDataset);
        GnuplotHelper::Write2dData("data-rate.data", rateDataset);
        GnuplotHelper::Write2dData("throughput.data", throughputDataset);
    }

private:
    Gnuplot m_powerPlot;
    Gnuplot m_ratePlot;
    Gnuplot m_throughputPlot;
};

void SetPosition(Ptr<Node> node, Vector position) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    mobility->SetPosition(position);
}

Vector GetPosition(Ptr<Node> node) {
    return node->GetObject<MobilityModel>()->GetPosition();
}

void ScheduleNextMove(Ptr<Node> staNode, uint32_t moveCount, uint32_t maxMoves, ThroughputTracker &tracker,
                      Ipv4Address serverIp, uint16_t port, Time simulationDuration, FlowMonitorHelper &flowmon,
                      PointToPointHelper &p2p, NodeContainer wifiNodes, NetDeviceContainer devices) {
    if (moveCount >= maxMoves) return;

    Simulator::Schedule(Seconds(1.0), &ScheduleNextMove, staNode, moveCount + 1, maxMoves, ref(tracker),
                        serverIp, port, simulationDuration, ref(flowmon), ref(p2p), ref(wifiNodes), ref(devices));

    Vector pos = GetPosition(staNode);
    pos.x += 1.0;
    SetPosition(staNode, pos);

    NS_LOG_INFO("Moving STA to x=" << pos.x);

    // Run traffic for 1 second before next move
    uint32_t currentMove = moveCount + 1;

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationDuration);

    UdpClientHelper client(serverIp, port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1472)); // UDP payload size

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(simulationDuration);

    flowmon.Install(wifiNodes);
    flowmon.Install(devices);

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataRate",
                    MakeCallback([](std::string path, DataRate rate) {
                        static double lastRate = 0.0;
                        if (rate.GetBitrate() != static_cast<uint64_t>(lastRate * 1e6)) {
                            lastRate = rate.GetBitrate() / 1e6;
                            NS_LOG_INFO("New data rate: " << lastRate << " Mbps");
                        }
                    }));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/TxPower",
                    MakeCallback([](std::string path, double txPower) {
                        NS_LOG_INFO("Current transmit power: " << txPower << " dBm");
                    }));

    Simulator::Run();

    // Calculate throughput
    FlowMonitor::FlowStatsContainer stats = flowmon.GetFlowStats();
    double totalRxBytes = 0;
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        totalRxBytes += it->second.rxBytes;
    }

    double throughput = totalRxBytes * 8.0 / (currentMove * 1e6); // Mbps
    double distance = pos.x;
    double avgPower = 20.0; // Placeholder - actual implementation would track this properly
    double avgRate = 54.0;  // Placeholder - actual implementation would track this properly

    tracker.LogThroughput(distance, throughput);
    tracker.LogTxParams(distance, avgPower, avgRate);
}

int main(int argc, char *argv[]) {
    std::string managerType = "ParfWifiManager";
    uint32_t rtsThreshold = 2346;
    std::string powerFile = "tx-power.plot";
    std::string rateFile = "data-rate.plot";
    std::string throughputFile = "throughput.plot";

    CommandLine cmd(__FILE__);
    cmd.AddValue("manager", "WiFi rate control manager (ParfWifiManager, AparfWifiManager, RrpaaWifiManager)", managerType);
    cmd.AddValue("rtsThreshold", "RTS threshold", rtsThreshold);
    cmd.AddValue("powerFile", "Output file for transmit power plot", powerFile);
    cmd.AddValue("rateFile", "Output file for data rate plot", rateFile);
    cmd.AddValue("throughputFile", "Output file for throughput plot", throughputFile);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsThreshold));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(2200));
    Config::SetDefault("ns3::WifiPhy::ChannelSettings", StringValue("{0, 0, 0, 0}"));

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Setup WiFi
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + managerType);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-comparison");

    NetDeviceContainer devices;

    // AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    devices = wifi.Install(phy, mac, wifiNodes.Get(0));

    // STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // Flow monitor
    FlowMonitorHelper flowmon;

    // Throughput tracking
    ThroughputTracker tracker(powerFile, rateFile, throughputFile);

    // P2P helper for client-server connections
    PointToPointHelper p2p;

    // Start the movement and measurement process
    ScheduleNextMove(wifiNodes.Get(1), 0, 20, tracker, interfaces.GetAddress(1), 9, Seconds(20.0), flowmon, p2p, wifiNodes, devices);

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    // Output results
    tracker.Write();

    return 0;
}