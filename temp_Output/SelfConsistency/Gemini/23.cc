#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBeThroughput");

int main(int argc, char* argv[]) {
    // Set default values for simulation parameters
    uint32_t mcs = 9;
    std::string frequency = "5"; // GHz
    bool uplinkOfdma = false;
    bool bsrp = false;
    uint32_t numStations = 2;
    std::string trafficType = "TCP";
    uint32_t payloadSize = 1472; // bytes
    uint32_t mpduBufferSize = 65535;
    std::string channelWidth = "160"; // MHz
    std::string guardInterval = "0.8"; // us
    double simulationTime = 1.0; // seconds
    double minExpectedThroughput = 100.0; // Mbps
    double maxExpectedThroughput = 1000.0; // Mbps
    double throughputTolerance = 0.1; // 10% tolerance

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue("mcs", "Modulation and Coding Scheme (MCS) value", mcs);
    cmd.AddValue("frequency", "Operating frequency (2.4, 5, or 6 GHz)", frequency);
    cmd.AddValue("uplinkOfdma", "Enable Uplink OFDMA (0 or 1)", uplinkOfdma);
    cmd.AddValue("bsrp", "Enable BSRP (0 or 1)", bsrp);
    cmd.AddValue("numStations", "Number of stations", numStations);
    cmd.AddValue("trafficType", "Traffic type (TCP or UDP)", trafficType);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
    cmd.AddValue("channelWidth", "Channel width (20, 40, 80, 160 MHz)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (0.8, 1.6, 3.2 us)", guardInterval);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("minExpectedThroughput", "Minimum expected throughput (Mbps)", minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput", "Maximum expected throughput (Mbps)", maxExpectedThroughput);
    cmd.AddValue("throughputTolerance", "Throughput tolerance (fraction)", throughputTolerance);

    cmd.Parse(argc, argv);

    // Log component enable
    LogComponentEnable("WifiBeThroughput", LOG_LEVEL_INFO);

    // Create nodes: one AP and specified number of stations
    NodeContainer apNodes;
    apNodes.Create(1);
    NodeContainer staNodes;
    staNodes.Create(numStations);

    // Configure Wi-Fi PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211be);

    // Configure Wi-Fi MAC
    WifiMacHelper mac;

    Ssid ssid = Ssid("wifi-be-network");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNodes);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Set the frequency
    std::stringstream ss;
    ss << "HtChannelNumber=";
    if (frequency == "2.4") {
        ss << 1; // 2.4 GHz
    } else if (frequency == "5") {
        ss << 36; // 5 GHz
    } else if (frequency == "6") {
        ss << 1; // 6 GHz
    } else {
        std::cerr << "Error: Invalid frequency specified.  Must be 2.4, 5, or 6." << std::endl;
        return 1;
    }

    YansWifiPhyHelper::SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Create Applications
    uint16_t port = 50000;
    Address serverAddress = InetSocketAddress(apInterface.GetAddress(0), port);

    ApplicationContainer sinkApps;
    if (trafficType == "TCP") {
      PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
      sinkApps = sinkHelper.Install(apNodes.Get(0));
    } else if (trafficType == "UDP") {
      PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", serverAddress);
      sinkApps = sinkHelper.Install(apNodes.Get(0));
    } else {
        std::cerr << "Error: Invalid traffic type specified. Must be TCP or UDP." << std::endl;
        return 1;
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime + 1));


    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numStations; ++i) {
        if (trafficType == "TCP") {
            BulkSendHelper sourceHelper("ns3::TcpSocketFactory", serverAddress);
            sourceHelper.SetAttribute("SendSize", UintegerValue(payloadSize));
            clientApps.Add(sourceHelper.Install(staNodes.Get(i)));
        } else {
            UdpClientHelper sourceHelper(serverAddress);
            sourceHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
            sourceHelper.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send forever
            clientApps.Add(sourceHelper.Install(staNodes.Get(i)));
        }
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));
    }


    // Configure MCS, channel width, and guard interval
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", StringValue(channelWidth));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", StringValue(guardInterval));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Mcs", UintegerValue(mcs));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    // Calculate throughput per station
    std::cout << "----------------------------------------------------------------------------------------" << std::endl;
    std::cout << " MCS | Channel Width | Guard Interval | Throughput (Mbps) |  Validation " << std::endl;
    std::cout << "----------------------------------------------------------------------------------------" << std::endl;
    double totalThroughput = 0.0;

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterface.GetAddress(0)) {
            double throughput = i->second.rxBytes * 8.0 / (simulationTime * 1000000.0); // Mbps
            totalThroughput += throughput;
            std::string validationResult = "PASS";
            if (throughput < minExpectedThroughput * (1 - throughputTolerance) || throughput > maxExpectedThroughput * (1 + throughputTolerance)) {
                validationResult = "FAIL";
                NS_LOG_ERROR("Throughput validation failed for MCS=" << mcs << ", Channel Width=" << channelWidth << ", Guard Interval=" << guardInterval << ", Throughput=" << throughput << " Mbps");
            }
            std::cout << " " << std::setw(3) << mcs << " | "
                      << std::setw(13) << channelWidth << " | "
                      << std::setw(14) << guardInterval << " | "
                      << std::setw(17) << std::fixed << std::setprecision(2) << throughput << " |  "
                      << validationResult << std::endl;
        }
    }

    std::cout << "----------------------------------------------------------------------------------------" << std::endl;

    monitor->SerializeToXmlFile("wifi-be-throughput.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}