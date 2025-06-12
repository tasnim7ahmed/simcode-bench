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
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7Simulation");

double CalculateThroughput(uint64_t bytes, double duration) {
    return (bytes * 8.0) / (duration * 1e6);
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nSta = 3;
    std::string trafficType = "TCP";
    uint32_t payloadSize = 1472;
    uint32_t mpduBufferSize = 8192;
    uint32_t mcs = 11;
    uint32_t channelWidth = 160;
    std::string guardInterval = "0.8";
    double simulationTime = 10.0;
    double startSimTime = 1.0;
    double stopSimTime = simulationTime - 1.0;
    double minExpectedThroughput = 100.0;
    double maxExpectedThroughput = 2000.0;
    double throughputTolerance = 0.1;
    bool uplinkOfdma = false;
    bool bsrp = false;
    uint32_t operatingFrequency = 5;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
    cmd.AddValue("nSta", "Number of wifi STA devices", nSta);
    cmd.AddValue("trafficType", "Traffic type (TCP or UDP)", trafficType);
    cmd.AddValue("payloadSize", "Payload size for UDP traffic", payloadSize);
    cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
    cmd.AddValue("mcs", "MCS value", mcs);
    cmd.AddValue("channelWidth", "Channel width (20, 40, 80, 160)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (0.8, 1.6, 3.2)", guardInterval);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("startSimTime", "Start traffic time", startSimTime);
    cmd.AddValue("stopSimTime", "Stop traffic time", stopSimTime);
    cmd.AddValue("minExpectedThroughput", "Minimum expected throughput", minExpectedThroughput);
    cmd.AddValue("maxExpectedThroughput", "Maximum expected throughput", maxExpectedThroughput);
    cmd.AddValue("throughputTolerance", "Throughput tolerance", throughputTolerance);
    cmd.AddValue("uplinkOfdma", "Enable Uplink OFDMA", uplinkOfdma);
    cmd.AddValue("bsrp", "Enable BSRP", bsrp);
    cmd.AddValue("operatingFrequency", "Operating frequency (2.4, 5, or 6)", operatingFrequency);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("Wifi7Simulation", LOG_LEVEL_INFO);
    }

    NodeContainer staNodes;
    staNodes.Create(nSta);

    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211be);

    WifiMacHelper mac;

    Ssid ssid = Ssid("wifi-7-network");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterfaces = address.Assign(apDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < nSta; ++i) {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port + i));
        PacketSinkHelper packetSinkHelper(trafficType == "TCP" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory", sinkLocalAddress);
        sinkApps.Add(packetSinkHelper.Install(staNodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nSta; ++i) {
        Address remoteAddress(InetSocketAddress(staNodeInterfaces.GetAddress(i), port + i));
        Ptr<Application> clientApp;

        if (trafficType == "TCP") {
            BulkSendHelper bulkSendHelper(trafficType == "TCP" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory", remoteAddress);
            bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
            clientApps.Add(bulkSendHelper.Install(apNode.Get(0)));
        } else {
            UdpClientHelper udpClientHelper(remoteAddress);
            udpClientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
            udpClientHelper.SetAttribute("MaxPackets", UintegerValue(0));
            clientApps.Add(udpClientHelper.Install(apNode.Get(0)));
        }

    }
    clientApps.Start(Seconds(startSimTime));
    clientApps.Stop(Seconds(stopSimTime));

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    bool errorFound = false;

    std::cout << "-----------------------------------------------------------------------------------------" << std::endl;
    std::cout << " MCS | Channel Width | GI  |  Throughput (Mbps) " << std::endl;
    std::cout << "-----------------------------------------------------------------------------------------" << std::endl;

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.destinationAddress == staNodeInterfaces.GetAddress(0) ||
            t.destinationAddress == staNodeInterfaces.GetAddress(1) ||
            t.destinationAddress == staNodeInterfaces.GetAddress(2)) {
            double throughput = CalculateThroughput(iter->second.rxBytes, stopSimTime - startSimTime);
            totalThroughput += throughput;

            std::cout << "  " << mcs << " |       " << channelWidth << "     | " << guardInterval << " |  " << throughput << std::endl;

            if (throughput < (minExpectedThroughput * (1 - throughputTolerance)) || throughput > (maxExpectedThroughput * (1 + throughputTolerance))) {
                std::cerr << "Error: Unexpected throughput for MCS " << mcs << ", Channel Width " << channelWidth << ", GI " << guardInterval << ": " << throughput << " Mbps" << std::endl;
                errorFound = true;
            }
        }
    }

    std::cout << "-----------------------------------------------------------------------------------------" << std::endl;

    if (errorFound) {
        Simulator::Destroy();
        return 1;
    }

    Simulator::Destroy();
    return 0;
}