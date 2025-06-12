#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7ThroughputSweep");

// Helper to parse string list, e.g., "20,40,80"
std::vector<uint16_t> ParseCsvToUint16(std::string str) {
    std::vector<uint16_t> values;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        values.push_back(static_cast<uint16_t>(std::stoi(token)));
    }
    return values;
}
std::vector<std::string> ParseCsvToString(std::string str) {
    std::vector<std::string> values;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        values.push_back(token);
    }
    return values;
}

// Throughput result holder
struct ThroughputResult {
    uint8_t mcs;
    uint16_t channelWidth;
    std::string gi;
    double throughputMbps;
};

// Default min/max throughput references for some MCS/width/GI, rough estimation for validation.
// (For illustration only—tuning is expected for proper experiments)
double GetMinThroughputBps(uint8_t mcs, uint16_t width, std::string gi, std::string trafficType) {
    double ref = 0.0;
    // These min levels are purposely low to tolerate overhead
    if (trafficType == "UDP") {
        ref = (width * 5e6 /* bandwidth */) * ((mcs+1)*0.25) * 0.2;
        if (gi == "short" || gi == "sgi") ref *= 1.04;
    } else { // TCP
        ref = (width * 5e6) * ((mcs+1)*0.20) * 0.10;
        if (gi == "short" || gi == "sgi") ref *= 1.04;
    }
    return ref;
}
double GetMaxThroughputBps(uint8_t mcs, uint16_t width, std::string gi) {
    double ref = (width * 5e6 /* bandwidth */) * ((mcs+1)*0.5);
    if (gi == "short" || gi == "sgi") ref *= 1.10;
    return ref;
}

// Utility: pick unique default channel for frequency
uint32_t GetChannelForFreq(std::string freq) {
    if (freq == "2.4") return 1;
    if (freq == "5") return 36;
    if (freq == "6") return 5;
    return 1;
}
WifiPhyStandard GetWifiStandardForFreq(std::string freq) {
    if (freq == "2.4") return WIFI_PHY_STANDARD_80211be;
    if (freq == "5")   return WIFI_PHY_STANDARD_80211be;
    if (freq == "6")   return WIFI_PHY_STANDARD_80211be;
    return WIFI_PHY_STANDARD_80211be;
}
uint16_t GetCenterFreqMHz(std::string freq) {
    if (freq == "2.4") return 2412;
    if (freq == "5")   return 5180;
    if (freq == "6")   return 5955;
    return 2412;
}

int main(int argc, char* argv[]) {
    // Parameters (defaults)
    std::string mcsListStr = "0,3,7,11";
    std::string channelWidthsStr = "20,40,80";
    std::string giListStr = "long,short";
    std::string freqListStr = "5";
    bool ulOfdma = false;
    bool bspr = false;
    uint32_t numStations = 2;
    std::string trafficType = "UDP"; // or "TCP"
    uint32_t payloadSize = 1470;
    uint32_t mpduBufferSize = 64;
    std::string auxPhySetStr = "";
    double simTime = 5.0;
    double tolerance = 0.20; // 20%
    bool verbose = false;

    // Parse cmdline args
    CommandLine cmd;
    cmd.AddValue("mcs", "CSV list of MCS values", mcsListStr);
    cmd.AddValue("channelWidths", "CSV list, e.g. 20,40,80", channelWidthsStr);
    cmd.AddValue("gi", "Guard Interval(s): long,short", giListStr);
    cmd.AddValue("frequencies", "Frequencies (2.4,5,6) as CSV", freqListStr);
    cmd.AddValue("ulOfdma", "Enable uplink OFDMA", ulOfdma);
    cmd.AddValue("bsrp", "Enable BSRP", bspr);
    cmd.AddValue("numStations", "Number of stations", numStations);
    cmd.AddValue("trafficType", "Traffic type: TCP or UDP", trafficType);
    cmd.AddValue("payloadSize", "Payload (bytes) per app data", payloadSize);
    cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
    cmd.AddValue("auxPhySettings", "Auxiliary PHY", auxPhySetStr);
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.AddValue("tolerance", "Allow % tolerance for throughput", tolerance);
    cmd.AddValue("verbose", "Print detailed Wi-Fi/log output", verbose);
    cmd.Parse(argc, argv);

    std::vector<uint8_t> mcsList;
    for (auto i : ParseCsvToUint16(mcsListStr)) mcsList.push_back(static_cast<uint8_t>(i));
    std::vector<uint16_t> channelWidths = ParseCsvToUint16(channelWidthsStr);
    std::vector<std::string> giList = ParseCsvToString(giListStr);
    std::vector<std::string> freqList = ParseCsvToString(freqListStr);

    // Table heading:
    std::cout << std::setw(6) << "MCS"
              << std::setw(12) << "ChWidth"
              << std::setw(10) << "GI"
              << std::setw(16) << "Throughput(Mbps)" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    for (const auto& freq : freqList) {
    for (auto mcs : mcsList) {
    for (auto width : channelWidths) {
    for (auto gi : giList) {

        if (verbose) {
            LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
            LogComponentEnable("WifiMac", LOG_LEVEL_INFO);
        }

        // --- Create nodes ---
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create(numStations);
        wifiApNode.Create(1);

        // --- Wi-Fi channel/Phy ---
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        phy.SetChannel(channel.Create());
        phy.Set("ChannelWidth", UintegerValue(width));
        phy.Set("Frequency", UintegerValue(GetCenterFreqMHz(freq)));
        phy.Set("GuardInterval", TimeValue(gi == "short" ? NanoSeconds(800) : NanoSeconds(1600)));
        phy.Set("RxAntenna", UintegerValue(1));
        phy.Set("TxAntenna", UintegerValue(1));

        // Additional PHY/aux param (just as example)
        if (!auxPhySetStr.empty()) {
            // Example: Auxiliary PHY; please adjust for specific settings
            // phy.Set("X", ValueOfAux);
        }

        // --- MAC / 802.11be ---
        WifiHelper wifi;
        wifi.SetStandard(GetWifiStandardForFreq(freq));

        WifiMacHelper mac;
        Ssid ssid = Ssid("wifi7-ssid");

        // Set up BE (11be) Rate Adaptation and force MCS:
        std::ostringstream oss;
        oss << "HeMcs" << unsigned(mcs);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue(oss.str()),
            "ControlMode", StringValue("HeMcs0")); // robust control

        NetDeviceContainer staDevices, apDevices;

        mac.SetType("ns3::StaWifiMac",
            "Ssid", SsidValue(ssid),
            "ActiveProbing", BooleanValue(false));
        staDevices = wifi.Install(phy, mac, wifiStaNodes);

        mac.SetType("ns3::ApWifiMac",
            "Ssid", SsidValue(ssid));
        apDevices = wifi.Install(phy, mac, wifiApNode);

        // --- Mobility ---
        MobilityHelper mobility;
        mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
            "MinX", DoubleValue (0.0),
            "MinY", DoubleValue (0.0),
            "DeltaX", DoubleValue (2.0),
            "DeltaY", DoubleValue (2.0),
            "GridWidth", UintegerValue (numStations+1),
            "LayoutType", StringValue ("RowFirst"));
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (wifiStaNodes);
        mobility.Install (wifiApNode);

        // --- Network Stack ---
        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNodes);

        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10." << unsigned(mcs) << "." << width << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer staInterfaces, apInterface;
        staInterfaces = address.Assign(staDevices);
        apInterface = address.Assign(apDevices);

        // --- Applications / Traffic ---
        uint16_t port = 5000;
        ApplicationContainer allApps;
        std::vector<Ptr<Application>> clientApps;
        std::vector<Ptr<Application>> serverApps;

        if (trafficType == "UDP") {
            // Each STA: OnOffClient->AP, UdpServer at AP
            UdpServerHelper udpServer(port);
            ApplicationContainer servApps = udpServer.Install(wifiApNode);
            servApps.Start(Seconds(0.5));
            servApps.Stop(Seconds(simTime+1.0));
            allApps.Add(servApps);
            for (uint32_t i=0; i<numStations; ++i) {
                UdpClientHelper udpClient(apInterface.GetAddress(0), port);
                udpClient.SetAttribute("MaxPackets", UintegerValue(0)); // unlimited
                udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
                udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
                ApplicationContainer cliApp = udpClient.Install(wifiStaNodes.Get(i));
                cliApp.Start(Seconds(1.0));
                cliApp.Stop(Seconds(simTime+1.0));
                allApps.Add(cliApp);
            }
        } else {
            // TCP: BulkSendHelper at STA->AP, PacketSink at AP
            uint32_t sinkPort = port;
            Address sinkLocalAddress (InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
            PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
            ApplicationContainer sinkApps = sinkHelper.Install(wifiApNode);
            sinkApps.Start(Seconds(0.5));
            sinkApps.Stop(Seconds(simTime+1.0));
            allApps.Add(sinkApps);

            for (uint32_t i=0; i<numStations; ++i) {
                BulkSendHelper source("ns3::TcpSocketFactory",
                    InetSocketAddress(apInterface.GetAddress(0), sinkPort));
                source.SetAttribute("MaxBytes", UintegerValue(0));
                source.SetAttribute("SendSize", UintegerValue(payloadSize));
                ApplicationContainer srcApp = source.Install(wifiStaNodes.Get(i));
                srcApp.Start(Seconds(1.0));
                srcApp.Stop(Seconds(simTime+1.0));
                allApps.Add(srcApp);
            }
        }

        // --- Enable FlowMonitor for accurate RX byte count ---
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        // --- Run Simulation ---
        Simulator::Stop(Seconds(simTime+2.0));
        Simulator::Run();

        // --- Measure throughput ---
        double totalRxBytes = 0.0;
        double throughputMbps = 0.0;
        // Per-station RX from AP
        Ptr<Ipv4FlowClassifier> classifier =
            DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

        for (auto& st : stats) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(st.first);
            bool isApRx = (t.destinationAddress == apInterface.GetAddress(0));
            if (trafficType == "UDP" && isApRx) {
                totalRxBytes += st.second.rxBytes;
            }
            if (trafficType == "TCP" && isApRx) {
                totalRxBytes += st.second.rxBytes;
            }
        }
        throughputMbps = (totalRxBytes * 8.0) / (simTime * 1e6);

        // --- Validation ---
        double minThr = GetMinThroughputBps(mcs, width, gi, trafficType) * (1.0-tolerance);
        double maxThr = GetMaxThroughputBps(mcs, width, gi) * (1.0+tolerance);

        if (throughputMbps*1e6 < minThr || throughputMbps*1e6 > maxThr) {
            std::cerr << "ERROR: Throughput for freq=" << freq
                      << " MCS=" << unsigned(mcs)
                      << " width=" << width << " GI=" << gi
                      << " out of range: " << throughputMbps
                      << " Mbps. [" << minThr/1e6 << " - " << maxThr/1e6 << " Mbps]\n";
            Simulator::Destroy();
            return 1;
        }

        // --- Output table line ---
        std::cout << std::setw(6) << unsigned(mcs)
                  << std::setw(12) << width
                  << std::setw(10) << gi
                  << std::setw(16) << std::fixed << std::setprecision(3) << throughputMbps
                  << std::endl;

        Simulator::Destroy();
    }}}}

    return 0;
}