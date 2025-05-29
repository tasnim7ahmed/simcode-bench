#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7ThroughputSimulation");

double CalculateThroughput(uint64_t bytes, double time) {
    return (bytes * 8.0) / (time * 1e6);
}

int main(int argc, char *argv[]) {
    // Enable command line arguments
    CommandLine cmd;
    uint32_t mcs = 11;
    double frequency = 5;
    bool uplinkOfdma = false;
    bool bsrp = false;
    uint32_t numStations = 1;
    std::string trafficType = "TCP";
    uint32_t payloadSize = 1472;
    uint32_t mpduBufferSize = 8192;
    double simulationTime = 10.0;
    uint32_t channelWidth = 80;
    std::string guardInterval = "0.8us";

    cmd.AddValue("mcs", "Modulation and Coding Scheme (0-11)", mcs);
    cmd.AddValue("frequency", "Operating frequency (2.4, 5, or 6 GHz)", frequency);
    cmd.AddValue("uplinkOfdma", "Enable Uplink OFDMA", uplinkOfdma);
    cmd.AddValue("bsrp", "Enable BSRP", bsrp);
    cmd.AddValue("numStations", "Number of stations", numStations);
    cmd.AddValue("trafficType", "Traffic type (TCP or UDP)", trafficType);
    cmd.AddValue("payloadSize", "Payload size", payloadSize);
    cmd.AddValue("mpduBufferSize", "MPDU buffer size", mpduBufferSize);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("channelWidth", "Channel Width (20, 40, 80, 160)", channelWidth);
    cmd.AddValue("guardInterval", "Guard Interval (0.8us, 1.6us, 3.2us)", guardInterval);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxMpdu", UintegerValue(mpduBufferSize));

    // Create nodes: AP and stations
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(numStations);

    // Configure Wi-Fi PHY
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211be);

    YgHtConfiguration htConfig;
    htConfig.SetGuardInterval(guardInterval);
    htConfig.SetChannelWidth(channelWidth);
    htConfig.SetMcs(mcs);

    wifiHelper.SetRemoteStationManager("ns3::YgHtWifiMac",
                                      "ChannelWidth", UintegerValue(channelWidth),
                                      "GuardInterval", StringValue(guardInterval),
                                      "BE_MaxAmpduSize", UintegerValue(8388607),
                                      "BE_UplinkOfdma", BooleanValue(uplinkOfdma),
                                      "BE_Bsrp", BooleanValue(bsrp),
                                      "BE_HtConfiguration", HtConfigurationValue(htConfig));
    
    WifiMacHelper macHelper;

    // Configure AP
    Ssid ssid = Ssid("wifi-7-network");
    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifiHelper.Install(macHelper, apNode);

    // Configure stations
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevices = wifiHelper.Install(macHelper, staNodes);

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
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Create Applications
    uint16_t port = 50000;
    ApplicationContainer appContainer;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));

    if (trafficType == "TCP") {
        BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", Address());
        bulkSendHelper.SetAttribute("SendSize", UintegerValue(payloadSize));

        for (uint32_t i = 0; i < numStations; ++i) {
            Address remoteAddress(InetSocketAddress(staInterfaces.GetAddress(i), port));
            bulkSendHelper.SetAttribute("Remote", AddressValue(remoteAddress));
            ApplicationContainer tempApp = bulkSendHelper.Install(apNode.Get(0));
            tempApp.Start(Seconds(1.0));
            tempApp.Stop(Seconds(simulationTime));
            appContainer.Add(tempApp);
        
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(staNodes);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime));
        appContainer.Add(sinkApp);

        }
    } else if (trafficType == "UDP") {
        UdpClientHelper udpClientHelper(staInterfaces.GetAddress(0), port);
        udpClientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
        udpClientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));

        for (uint32_t i = 0; i < numStations; ++i) {
            ApplicationContainer tempApp = udpClientHelper.Install(apNode.Get(0));
             tempApp.Start(Seconds(1.0));
             tempApp.Stop(Seconds(simulationTime));
            appContainer.Add(tempApp);
        }

         PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(staNodes);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime));
        appContainer.Add(sinkApp);

    } else {
        std::cerr << "Invalid traffic type: " << trafficType << std::endl;
        return 1;
    }

    // PHY configuration (frequency)
    YgHtWifiPhy ygHtWifiPhy = YgHtWifiPhy();

    if (frequency == 2.4) {
        ygHtWifiPhy.SetChannelNumber(1);
    } else if (frequency == 5) {
        ygHtWifiPhy.SetChannelNumber(36);
    } else if (frequency == 6) {
        ygHtWifiPhy.SetChannelNumber(5);
    } else {
        std::cerr << "Invalid frequency: " << frequency << std::endl;
        return 1;
    }

   
    //Flow monitor
     FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();
    
    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate throughput
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    double totalBytesReceived = 0;
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        const Ipv4FlowClassifier::FiveTuple &tuple = classifier->FindFlow(it->first);
        if(tuple.destinationPort == port){
            totalBytesReceived += it->second.rxBytes;
        }
    }

    double throughput = CalculateThroughput(totalBytesReceived, simulationTime);

    // Print results
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << "MCS\tChannel Width\tGI\tThroughput (Mbps)" << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    std::cout << mcs << "\t\t" << channelWidth << "\t\t" << guardInterval << "\t\t" << throughput << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;

    Simulator::Destroy();
    return 0;
}