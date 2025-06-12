#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughput");

class ThroughputData {
public:
    std::map<uint8_t, std::vector<std::pair<double, double>>> data; // MCS -> vector of (distance, throughput)
};

void CalculateThroughput(Ptr<FlowMonitor> flowMonitor, Ptr<Ipv4FlowClassifier> classifier,
                         ThroughputData& throughputData, uint8_t mcs, double distance) {
    double totalRxBytes = 0;
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        if (t.destinationPort == 9) {
            totalRxBytes += it->second.rxBytes;
        }
    }

    double throughput = (totalRxBytes * 8.0) / 1e6; // Mbps
    throughputData.data[mcs].push_back(std::make_pair(distance, throughput));
    NS_LOG_INFO("Distance: " << distance << "m, MCS: " << (uint32_t)mcs << ", Throughput: " << throughput << " Mbps");
}

int main(int argc, char *argv[]) {
    // Parameters
    bool useTcp = false;
    double simulationTime = 5.0;
    double minDistance = 1.0;
    double maxDistance = 100.0;
    double distanceStep = 5.0;
    double frequency = 5.0;
    uint16_t channelWidth = 40;
    bool shortGuardInterval = true;
    bool enablePreambleDetection = true;
    bool channelBonding = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.AddValue("simulationTime", "Duration of simulation in seconds", simulationTime);
    cmd.AddValue("minDistance", "Minimum distance between AP and station", minDistance);
    cmd.AddValue("maxDistance", "Maximum distance between AP and station", maxDistance);
    cmd.AddValue("distanceStep", "Step size for varying distance", distanceStep);
    cmd.AddValue("frequency", "Wi-Fi frequency band (2.4 or 5.0 GHz)", frequency);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("shortGuardInterval", "Enable short guard interval", shortGuardInterval);
    cmd.AddValue("enablePreambleDetection", "Enable preamble detection", enablePreambleDetection);
    cmd.AddValue("channelBonding", "Enable channel bonding", channelBonding);
    cmd.Parse(argc, argv);

    ThroughputData throughputData;

    // MCS values from 0 to 31
    for (uint8_t mcs = 0; mcs <= 31; ++mcs) {
        for (double distance = minDistance; distance <= maxDistance; distance += distanceStep) {
            RngSeedManager::SetSeed(1);
            RngSeedManager::SetRun(1);

            NodeContainer wifiStaNode;
            wifiStaNode.Create(1);
            NodeContainer wifiApNode;
            wifiApNode.Create(1);

            YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
            YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
            phyHelper.SetChannel(channelHelper.Create());
            phyHelper.Set("Frequency", DoubleValue(frequency * 1e9));
            phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));
            phyHelper.Set("ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
            phyHelper.Set("PreambleDetectionSupported", BooleanValue(enablePreambleDetection));

            WifiMacHelper macHelper;
            WifiHelper wifiHelper;
            wifiHelper.SetStandard(WIFI_STANDARD_80211n);
            wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                              "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                                              "ControlMode", StringValue("HtMcs0"));

            // MIMO configuration based on MCS index
            if (mcs >= 0 && mcs <= 7) {
                wifiHelper.SetNumberOfTransmitAntennas(1);
                wifiHelper.SetNumberOfReceiveAntennas(1);
            } else if (mcs >= 8 && mcs <= 15) {
                wifiHelper.SetNumberOfTransmitAntennas(2);
                wifiHelper.SetNumberOfReceiveAntennas(2);
            } else if (mcs >= 16 && mcs <= 23) {
                wifiHelper.SetNumberOfTransmitAntennas(3);
                wifiHelper.SetNumberOfReceiveAntennas(3);
            } else if (mcs >= 24 && mcs <= 31) {
                wifiHelper.SetNumberOfTransmitAntennas(4);
                wifiHelper.SetNumberOfReceiveAntennas(4);
            }

            Ssid ssid = Ssid("mimo-network");
            macHelper.SetType("ns3::StaWifiMac",
                              "Ssid", SsidValue(ssid),
                              "ActiveProbing", BooleanValue(false));

            NetDeviceContainer staDevices = wifiHelper.Install(phyHelper, macHelper, wifiStaNode);

            macHelper.SetType("ns3::ApWifiMac",
                              "Ssid", SsidValue(ssid));

            NetDeviceContainer apDevices = wifiHelper.Install(phyHelper, macHelper, wifiApNode);

            // Mobility
            MobilityHelper mobility;
            mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                         "MinX", DoubleValue(0.0),
                                         "MinY", DoubleValue(0.0),
                                         "DeltaX", DoubleValue(distance),
                                         "DeltaY", DoubleValue(0.0),
                                         "GridWidth", UintegerValue(2),
                                         "LayoutType", StringValue("RowFirst"));
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(wifiStaNode);
            mobility.Install(wifiApNode);

            // Internet stack
            InternetStackHelper stack;
            stack.Install(wifiStaNode);
            stack.Install(wifiApNode);

            Ipv4AddressHelper address;
            address.SetBase("192.168.1.0", "255.255.255.0");
            Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
            Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

            Address sinkAddress;
            uint16_t port = 9;

            if (useTcp) {
                PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
                sinkApp.Start(Seconds(0.0));
                sinkApp.Stop(Seconds(simulationTime));

                OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
                client.SetAttribute("MaxBytes", UintegerValue(0));
                client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
                clientApp.Start(Seconds(1.0));
                clientApp.Stop(Seconds(simulationTime - 0.1));
                sinkAddress = InetSocketAddress(apInterface.GetAddress(0), port);
            } else {
                UdpServerHelper server(port);
                ApplicationContainer sinkApp = server.Install(wifiApNode.Get(0));
                sinkApp.Start(Seconds(0.0));
                sinkApp.Stop(Seconds(simulationTime));

                UdpClientHelper client(apInterface.GetAddress(0), port);
                client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
                client.SetAttribute("PacketSize", UintegerValue(1400));
                ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
                clientApp.Start(Seconds(1.0));
                clientApp.Stop(Seconds(simulationTime - 0.1));
                sinkAddress = InetSocketAddress(apInterface.GetAddress(0), port);
            }

            FlowMonitorHelper flowmonHelper;
            Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

            Simulator::Stop(Seconds(simulationTime));
            Simulator::Run();

            CalculateThroughput(flowMonitor, DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier()), throughputData, mcs, distance);

            Simulator::Destroy();
        }
    }

    // Plotting with Gnuplot
    Gnuplot plot("wifi-mimo-throughput.png");
    plot.SetTitle("Throughput vs Distance for 802.11n MIMO Streams");
    plot.Set xlabel("Distance (m)");
    plot.Set ylabel("Throughput (Mbps)");
    plot.Set terminal("png");

    std::vector<GnuplotDataset> datasets;
    for (const auto &entry : throughputData.data) {
        GnuplotDataset dataset;
        dataset.title = "MCS " + std::to_string(entry.first);
        dataset.style = "linespoints";
        for (const auto &point : entry.second) {
            dataset.data.push_back(Gnuplot2dDataset::DataPoint(point.first, point.second));
        }
        datasets.push_back(dataset);
    }

    std::ofstream plotFile("wifi-mimo-throughput.plt");
    plot.GenerateOutput(plotFile, datasets);
    plotFile.close();

    return 0;
}