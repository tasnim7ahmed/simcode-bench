#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-helper.h"
#include <vector>
#include <sstream>
#include <iomanip>

using namespace ns3;

// Utility function to get all HT MCS index for nStreams (see 802.11n spec)
std::vector<uint8_t> GetMcsList(uint8_t nStreams)
{
    std::vector<uint8_t> mcs;
    // Based on Table 20-28 of 802.11-2016, maximum supported MCS for nstreams.
    // For simplicity, use MCS 0 up to 7 for each stream. MCS idx = NSS*8-1 for 1..4 streams
    uint8_t maxMcs = nStreams * 8 - 1;
    for (uint8_t i = (nStreams-1)*8; i <= maxMcs; ++i)
        mcs.push_back(i);
    return mcs;
}

struct ThroughputResult
{
    double distance;
    std::vector<double> throughputMbps;
};

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Simulation parameters
    double minDistance = 1.0;            // in meters
    double maxDistance = 100.0;          // in meters
    double stepDistance = 5.0;           // meters
    double simulationTime = 10.0;        // seconds
    std::string phyStandard = "802.11n";
    std::string udpOrTcp = "UDP";        // "UDP" or "TCP"
    std::string band = "5.0";            // "2.4" or "5.0"
    uint32_t channelWidth = 20;          // MHz (20, 40)
    bool shortGi = false;
    bool greenfield = false;
    bool sgiPreamble = false;
    uint32_t maxNStreams = 4;
    std::string channelBonding = "NONE"; // NONE, HT40P, HT40M, according to ns-3
    uint16_t port = 5001;
    std::string dataRate = "200Mbps";    // arbitrary high rate, avoid bottleneck at app
    uint32_t packetSize = 1472;          // UDP payload

    // Read command line
    CommandLine cmd;
    cmd.AddValue("minDistance", "Min distance (m)", minDistance);
    cmd.AddValue("maxDistance", "Max distance (m)", maxDistance);
    cmd.AddValue("stepDistance", "Step size (m)", stepDistance);
    cmd.AddValue("simulationTime", "Simulation time per run (s)", simulationTime);
    cmd.AddValue("udpOrTcp", "Traffic type: UDP or TCP", udpOrTcp);
    cmd.AddValue("band", "Frequency band: '2.4' or '5.0' GHz", band);
    cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
    cmd.AddValue("shortGi", "Use Short Guard Interval", shortGi);
    cmd.AddValue("greenfield", "Use Greenfield preamble", greenfield);
    cmd.AddValue("sgiPreamble", "Use Short GI in preamble detection", sgiPreamble);
    cmd.AddValue("maxNStreams", "Max MIMO streams (1-4)", maxNStreams);
    cmd.AddValue("channelBonding", "Channel bonding (NONE/HT40P/HT40M)", channelBonding);
    cmd.Parse(argc, argv);

    int bandIdx = (band == "5.0") ? 1 : 0;
    double freqList[2] = {2.4e9, 5.0e9};
    uint16_t channelNumberList[2] = {1, 36};
    std::string phyBandList[2] = {"HtMcs", "HtMcs"};

    std::vector<std::vector<ThroughputResult>> results; // [nStreams-1][mcsIdx]
    std::vector<std::vector<std::vector<double>>> allThroughput; // [nStreams-1][mcsIdx][distanceIdx]
    std::vector<std::vector<std::string>>      mcsLabels; // [nStreams-1][mcsIdx]
    std::vector<double> distances;
    uint32_t nDistances = 0;
    for (double d = minDistance; d <= maxDistance; d += stepDistance)
    {
        distances.push_back(d);
        ++nDistances;
    }

    // For labels
    std::vector<std::string> streamLabels;
    for (uint8_t nStreams = 1; nStreams <= maxNStreams; ++nStreams)
    {
        std::ostringstream oss;
        oss << nStreams << "x" << nStreams << " MIMO";
        streamLabels.push_back(oss.str());
    }

    // Preallocate result vectors
    allThroughput.resize(maxNStreams);
    mcsLabels.resize(maxNStreams);
    for (uint8_t nStreams = 1; nStreams <= maxNStreams; ++nStreams)
    {
        std::vector<uint8_t> mcsList = GetMcsList(nStreams);
        allThroughput[nStreams-1].resize(mcsList.size());
        mcsLabels[nStreams-1].resize(mcsList.size());
        for (size_t mcsIdx = 0; mcsIdx < mcsList.size(); ++mcsIdx)
        {
            allThroughput[nStreams-1][mcsIdx].resize(nDistances, 0.0);
            std::ostringstream oss;
            oss << "NSS=" << int(nStreams) << ",MCS=" << int(mcsList[mcsIdx]);
            mcsLabels[nStreams-1][mcsIdx] = oss.str();
        }
    }

    uint32_t distanceIdx = 0;
    for (double distance : distances)
    {
        NS_LOG_UNCOND("Simulating distance: " << distance << " m");
        uint8_t nStreamsIdx = 0;
        for (uint8_t nStreams = 1; nStreams <= maxNStreams; ++nStreams)
        {
            uint32_t mcsIdx = 0;
            std::vector<uint8_t> mcsList = GetMcsList(nStreams);
            for (uint8_t mcs : mcsList)
            {
                // Reset simulator state per scenario
                Simulator::Stop();
                Simulator::Destroy();

                // Create nodes
                NodeContainer wifiStaNode, wifiApNode;
                wifiStaNode.Create(1);
                wifiApNode.Create(1);

                // Setup WiFi PHY+MAC
                YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
                YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
                phy.SetChannel(channel.Create());
                phy.Set("ShortGuardEnabled", BooleanValue(shortGi));
                phy.Set("GreenfieldEnabled", BooleanValue(greenfield));
                phy.Set("ShortPreambleDetection", BooleanValue(sgiPreamble));
                phy.Set("ChannelWidth", UintegerValue(channelWidth));
                phy.Set("Frequency", UintegerValue((uint32_t)(freqList[bandIdx]/1e6)));
                phy.Set("ChannelNumber", UintegerValue(channelNumberList[bandIdx]));

                WifiHelper wifi;
                wifi.SetStandard(WIFI_STANDARD_80211n);

                // Set MIMO and HT configs
                wifi.SetRemoteStationManager(
                    "ns3::ConstantRateWifiManager",
                    "DataMode", StringValue("HtMcs" + std::to_string(mcs)),
                    "ControlMode", StringValue("HtMcs0"));

                // MIMO spatial streams
                HtConfiguration htconfig;
                htconfig.SetMcsSupported(0, false);
                std::vector<bool> mcsMap (32, false);
                mcsMap[mcs] = true;
                htconfig.SetMcsSet(mcsMap);

                phy.Set("Antennas", UintegerValue(nStreams));
                phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(nStreams));
                phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(nStreams));

                Ssid ssid = Ssid("ns3-80211n-mimo");

                WifiMacHelper mac;

                mac.SetType("ns3::StaWifiMac",
                            "Ssid", SsidValue(ssid),
                            "ActiveProbing", BooleanValue(false));
                NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

                mac.SetType("ns3::ApWifiMac",
                            "Ssid", SsidValue(ssid));
                NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

                // Mobility
                MobilityHelper mobility;
                mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                             "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                             "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(0.0),
                                             "GridWidth", UintegerValue(1),
                                             "LayoutType", StringValue("RowFirst"));

                mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                mobility.Install(wifiApNode);
                mobility.Install(wifiStaNode);

                // Set positions
                Ptr<MobilityModel> apMob = wifiApNode.Get(0)->GetObject<MobilityModel>();
                Ptr<MobilityModel> staMob = wifiStaNode.Get(0)->GetObject<MobilityModel>();
                apMob->SetPosition(Vector(0.0, 0.0, 1.0));
                staMob->SetPosition(Vector(distance, 0.0, 1.0));

                // Internet stack
                InternetStackHelper stack;
                stack.Install(wifiApNode);
                stack.Install(wifiStaNode);

                Ipv4AddressHelper address;
                address.SetBase("10.1.1.0", "255.255.255.0");
                Ipv4InterfaceContainer staNodeInterface, apNodeInterface;
                apNodeInterface = address.Assign(apDevice);
                staNodeInterface = address.Assign(staDevice);

                // Application setup
                double appStart = 1.0;
                double appStop  = simulationTime + 1.0;

                // Throughput measurement
                uint64_t totalRxBytes = 0;
                ApplicationContainer serverApp, clientApp;

                if (udpOrTcp == "UDP")
                {
                    // Use UdpServer on STA, UdpClient on AP
                    UdpServerHelper udpServer(port);
                    serverApp = udpServer.Install(wifiStaNode);
                    serverApp.Start(Seconds(appStart));
                    serverApp.Stop(Seconds(appStop));

                    UdpClientHelper udpClient(staNodeInterface.GetAddress(0), port);
                    udpClient.SetAttribute("MaxPackets", UintegerValue(0));
                    udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
                    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
                    udpClient.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
                    udpClient.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
                    clientApp = udpClient.Install(wifiApNode);
                }
                else // TCP
                {
                    // TCP Sink on STA, TCP BulkSend on AP
                    uint16_t tcpPort = port;
                    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
                    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
                    serverApp = packetSinkHelper.Install(wifiStaNode);
                    serverApp.Start(Seconds(appStart));
                    serverApp.Stop(Seconds(appStop));

                    BulkSendHelper bulkSender("ns3::TcpSocketFactory", InetSocketAddress(staNodeInterface.GetAddress(0), tcpPort));
                    bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
                    bulkSender.SetAttribute("SendSize", UintegerValue(packetSize));
                    bulkSender.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
                    bulkSender.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
                    clientApp = bulkSender.Install(wifiApNode);
                }

                // FlowMonitor for accurate throughput
                FlowMonitorHelper flowmon;
                Ptr<FlowMonitor> monitor = flowmon.InstallAll();

                Simulator::Stop(Seconds(appStop + 1.0));
                Simulator::Run();

                // Throughput calculation
                double throughput = 0.0;
                monitor->CheckForLostPackets();
                Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
                std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

                for (auto flow : stats)
                {
                    auto fiveTuple = classifier->FindFlow(flow.first);
                    if ((udpOrTcp == "UDP"  && fiveTuple.destinationPort == port) ||
                        (udpOrTcp == "TCP"  && fiveTuple.destinationPort == port))
                    {
                        double timeTx = (flow.second.timeLastRxPacket.GetSeconds()-appStart);
                        if (timeTx > 0)
                        {
                            throughput = (flow.second.rxBytes * 8.0) / (timeTx * 1e6); // Mbps
                        }
                        break;
                    }
                }

                allThroughput[nStreamsIdx][mcsIdx][distanceIdx] = throughput;

                Simulator::Destroy();
                ++mcsIdx;
            }
            ++nStreamsIdx;
        }
        ++distanceIdx;
    }

    // --- Gnuplot Output ---
    Gnuplot plot("wifi-80211n-mimo-throughput-vs-distance.png");
    plot.SetTitle("802.11n Throughput vs. Distance for HT MCS / MIMO streams");
    plot.SetTerminal("png size 1280,960");
    plot.SetLegend("Distance (m)", "Throughput (Mbps)");
    plot.AppendExtra("set grid");

    for (uint8_t nStreams = 1; nStreams <= maxNStreams; ++nStreams)
    {
        std::vector<uint8_t> mcsList = GetMcsList(nStreams);
        for (size_t mcsIdx = 0; mcsIdx < mcsList.size(); ++mcsIdx)
        {
            // Prepare data series
            Gnuplot2dDataset dataset;
            std::ostringstream label;
            label << "NSS=" << int(nStreams) << ",MCS=" << int(mcsList[mcsIdx]);
            dataset.SetTitle(label.str());
            dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

            for (distanceIdx = 0; distanceIdx < nDistances; ++distanceIdx)
            {
                double th = allThroughput[nStreams-1][mcsIdx][distanceIdx];
                dataset.Add(distances[distanceIdx], th);
            }
            plot.AddDataset(dataset);
        }
    }

    std::ofstream plotFile("wifi-80211n-mimo-throughput-vs-distance.plt");
    plot.GenerateOutput(plotFile);
    plotFile.close();

    NS_LOG_UNCOND("Simulation completed. See 'wifi-80211n-mimo-throughput-vs-distance.plt' and PNG plot.");

    return 0;
}