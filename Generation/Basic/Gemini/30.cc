#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ht-wifi-mac-helper.h"
#include "ns3/ht-wifi-phy-helper.h"

#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. Set up configurable parameters with default values
    uint32_t nStations = 1;
    uint32_t mcs = 7; // HT MCS value (0-7)
    uint32_t channelWidth = 40; // Channel width (20 or 40 MHz)
    std::string guardIntervalStr = "short"; // Guard interval (long or short)
    double distance = 10.0; // Distance between stations and AP in meters
    bool rtsCts = false; // Whether RTS/CTS is used
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1472; // bytes
    std::string dataRate = "100Mbps"; // Application data rate (should be high enough not to limit WiFi)

    // 2. Command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of Wi-Fi stations", nStations);
    cmd.AddValue("mcs", "HT MCS value (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (long or short)", guardIntervalStr);
    cmd.AddValue("distance", "Distance between stations and AP (meters)", distance);
    cmd.AddValue("rtsCts", "Enable RTS/CTS (true/false)", rtsCts);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("packetSize", "UDP packet size (bytes)", packetSize);
    cmd.AddValue("dataRate", "UDP application data rate", dataRate);
    cmd.Parse(argc, argv);

    // Validate parameters
    if (mcs > 7) {
        NS_FATAL_ERROR("MCS value must be between 0 and 7 for HT modes.");
    }
    if (channelWidth != 20 && channelWidth != 40) {
        NS_FATAL_ERROR("Channel width must be 20 or 40 MHz.");
    }
    if (guardIntervalStr != "long" && guardIntervalStr != "short") {
        NS_FATAL_ERROR("Guard interval must be 'long' or 'short'.");
    }

    // 3. Create nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(nStations);

    // 4. Configure mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP at origin

    for (uint32_t i = 0; i < nStations; ++i) {
        // Place stations in a line at the specified distance, with small Y-offset
        positionAlloc->Add(Vector(distance, i * 0.1, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    // 5. Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // Using 5GHz band for 802.11n

    // Configure HT PHY (802.11n)
    HtWifiPhyHelper wifiPhy = HtWifiPhyHelper::Default();
    wifiPhy.SetChannel("ns3::YansWifiChannel");
    wifiPhy.SetChannelWidth(channelWidth);
    wifiPhy.SetGuardInterval(guardIntervalStr == "short" ? WifiPhyHelper::GI_SHORT : WifiPhyHelper::GI_LONG);
    wifiPhy.Set80211nReferenced(true); // Must be true for 802.11n specific configurations
    wifiPhy.SetPhyMode(HtWifiPhyHelper::GetPhyMode(mcs));

    // Configure HT MAC
    HtWifiMacHelper wifiMac = HtWifiMacHelper::Default();
    wifiMac.SetSlottedTime(true); // Always for HT

    // Set RTS/CTS threshold
    if (rtsCts) {
        wifiMac.SetRtsCtsThreshold(0); // Enable for all packets
    } else {
        wifiMac.SetRtsCtsThreshold(999999); // Effectively disable
    }

    // Install Wi-Fi devices
    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ns3-wifi")));
    apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ns3-wifi")), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    // 6. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // 7. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces;
    Ipv4InterfaceContainer staInterfaces;

    apInterfaces = address.Assign(apDevices);
    staInterfaces = address.Assign(staDevices);

    // 8. Traffic generation (UDP) with multiple TOS values
    // Define TOS values to cycle through
    std::vector<uint8_t> tosValues = {0x00, 0x28, 0xA0}; // Best Effort (0), Expedited Forwarding (0x28), Control (0xA0)

    // AP acts as UDP server
    uint16_t port = 9; // Discard port
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1.0));

    // Stations act as UDP clients
    for (uint32_t i = 0; i < nStations; ++i) {
        UdpClientHelper client(apInterfaces.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0)); // Send indefinitely
        client.SetAttribute("Interval", TimeValue(NanoSeconds(packetSize * 8 / DataRate(dataRate).GetBitRate())));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        
        // Set TOS value for this client (cycling through defined TOS values)
        uint8_t currentTos = tosValues[i % tosValues.size()];
        client.SetAttribute("TosField", UintegerValue(currentTos));

        ApplicationContainer clientApps = client.Install(staNodes.Get(i));
        clientApps.Start(Seconds(2.0)); // Start clients after server
        clientApps.Stop(Seconds(simulationTime));
    }

    // 9. Flow Monitor for throughput measurement
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // 10. Simulation execution
    Simulator::Stop(Seconds(simulationTime + 2.0)); // Stop after all applications have stopped + a small buffer
    Simulator::Run();

    // 11. Calculate and output aggregated UDP throughput
    monitor->CheckForFlows(); // Explicitly check for flows before getting stats
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalRxBytes = 0;
    // Iterate through flows and sum up received UDP bytes
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = flowHelper.GetClassifier()->Get
        if (t.protocol == 17) { // UDP Protocol number
            totalRxBytes += i->second.rxBytes;
        }
    }

    double aggregatedThroughputMbps = (totalRxBytes * 8.0) / (simulationTime * 1000000.0);

    std::cout << "\n--- Simulation Results ---" << std::endl;
    std::cout << "Number of stations: " << nStations << std::endl;
    std::cout << "HT MCS: " << mcs << std::endl;
    std::cout << "Channel Width: " << channelWidth << " MHz" << std::endl;
    std::cout << "Guard Interval: " << guardIntervalStr << std::endl;
    std::cout << "Distance: " << distance << " m" << std::endl;
    std::cout << "RTS/CTS Enabled: " << (rtsCts ? "Yes" : "No") << std::endl;
    std::cout << "Simulation Time: " << simulationTime << " s" << std::endl;
    std::cout << "Aggregated UDP Throughput: " << aggregatedThroughputMbps << " Mbps" << std::endl;
    
    // Clean up
    Simulator::Destroy();

    return 0;
}