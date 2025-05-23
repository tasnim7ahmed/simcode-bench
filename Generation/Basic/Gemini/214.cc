#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiStaApSimulation");

int main(int argc, char *argv[])
{
    // 1. Command line arguments and configuration
    uint32_t nSta = 3; // Number of STA nodes (excluding the AP)
    double simulationTime = 10.0; // Total simulation time in seconds
    std::string phyMode = "HtMcs7"; // 802.11n MCS Index 7 (for 65 Mbps in 20MHz BW)
    std::string tracingPath = "wifi-simulation"; // Base name for pcap files

    CommandLine cmd(__FILE__);
    cmd.AddValue("nSta", "Number of STA nodes (default: 3)", nSta);
    cmd.AddValue("simulationTime", "Total simulation time in seconds (default: 10.0)", simulationTime);
    cmd.AddValue("phyMode", "Wi-Fi Phy mode (e.g., HtMcs7, OfdmRate6Mbps)", phyMode);
    cmd.Parse(argc, argv);

    // Ensure there are enough STA nodes for the specified communication pattern
    if (nSta < 2)
    {
        NS_FATAL_ERROR("At least 2 STA nodes are required for inter-STA communication (STA1->STA2).");
    }

    // 2. Logging setup
    LogComponentEnable("WifiStaApSimulation", LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO); // Enable for verbose application logs
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO); // Enable for verbose packet sink logs

    // 3. Configure Wi-Fi and application parameters
    // Set default packet size and application data rate
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(1024)); // 1KB packets
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("30Mbps")); // 30 Mbps desired data rate for applications
    // Set the non-unicast (broadcast/multicast) mode for Wi-Fi, often used for beacons
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    // 4. Create Nodes
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(nSta);

    // 5. Mobility Model (AP at origin, STAs spread out)
    MobilityHelper mobility;
    
    // AP mobility (fixed at origin)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    apNode.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // STA mobility (fixed positions around the AP)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (uint32_t i = 0; i < nSta; ++i)
    {
        Ptr<ConstantPositionMobilityModel> positionModel = staNodes.Get(i)->GetObject<ConstantPositionMobilityModel>();
        if (positionModel == nullptr)
        {
            positionModel = CreateObject<ConstantPositionMobilityModel>();
            staNodes.Get(i)->AggregateObject(positionModel);
        }

        // Place STAs at different fixed locations to simulate real-world distances
        // (0.0, 10.0, 0.0) -> STA1
        // (0.0, -10.0, 0.0) -> STA2
        // (10.0, 0.0, 0.0) -> STA3
        if (i == 0)
        {
            positionModel->SetPosition(Vector(0.0, 10.0, 0.0));
        }
        else if (i == 1)
        {
            positionModel->SetPosition(Vector(0.0, -10.0, 0.0));
        }
        else if (i == 2)
        {
            positionModel->SetPosition(Vector(10.0, 0.0, 0.0));
        }
        else
        {
            // For nSta > 3, subsequent STAs will be placed at (0, -10.0, 0.0) as an example
            // In this specific problem, nSta is 3, so this case won't be hit.
            positionModel->SetPosition(Vector(-10.0, 0.0, 0.0));
        }
    }
    mobility.Install(staNodes);

    // 6. Wi-Fi Channel and PHY (physical layer)
    WifiHelper wifiHelper;
    // Use 802.11n standard in 5GHz band
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHZ); 

    YansWifiChannelHelper channel;
    // LogDistancePropagationLossModel for path loss, ConstantPropagationDelayModel for delay
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue(3.0)); // Path loss exponent
    channel.SetPropagationDelayModel("ns3::ConstantPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phyHelper;
    phyHelper.SetChannel(wifiChannel);
    // Set constant transmit power for all devices
    phyHelper.Set("TxPowerStart", DoubleValue(16.0)); // dBm
    phyHelper.Set("TxPowerEnd", DoubleValue(16.0));   // dBm

    // 7. Wi-Fi MAC layer
    // NqosWifiMacHelper is suitable for 802.11n/ac non-QoS networks
    NqosWifiMacHelper macHelper = NqosWifiMacHelper::Default(); 

    Ssid ssid = Ssid("ns3-wifi"); // Define the network SSID

    // Configure AP MAC attributes
    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "BeaconInterval", TimeValue(MicroSeconds(102400))); // 100ms beacon interval
    NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    // Configure STA MAC attributes
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false)); // STAs passively listen for beacons
    NetDeviceContainer staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    // 8. Install Internet Stack (IPv4)
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // 9. Assign IP Addresses to Wi-Fi devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0"); // Network 10.1.1.0/24
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Enable static global routing to allow inter-STA communication through the AP
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 10. Applications: Stations communicating with each other through the AP
    // Traffic Pattern: STA1 (Node 0 in staNodes) -> STA2 (Node 1 in staNodes)
    //                  STA2 (Node 1 in staNodes) -> STA3 (Node 2 in staNodes)
    
    uint16_t port1 = 9;  // Port for STA1 -> STA2 flow
    uint16_t port2 = 10; // Port for STA2 -> STA3 flow

    // Application 1: OnOffApplication from STA1 to PacketSink on STA2
    Ptr<Node> senderNode1 = staNodes.Get(0);
    Ptr<Node> receiverNode1 = staNodes.Get(1);
    Ipv4Address receiverAddress1 = staInterfaces.GetAddress(1); // IP address of STA2

    OnOffHelper onoff1("ns3::UdpSocketFactory",
                       InetSocketAddress(receiverAddress1, port1));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always ON
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never OFF
    ApplicationContainer clientApps1 = onoff1.Install(senderNode1);
    clientApps1.Start(Seconds(1.0)); // Start after 1s (network settlement)
    clientApps1.Stop(Seconds(simulationTime - 1.0)); // Stop 1s before simulation end

    PacketSinkHelper sink1("ns3::UdpSocketFactory",
                           InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer serverApps1 = sink1.Install(receiverNode1);
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(simulationTime));

    // Application 2: OnOffApplication from STA2 to PacketSink on STA3 (if enough STAs)
    if (nSta >= 3)
    {
        Ptr<Node> senderNode2 = staNodes.Get(1);
        Ptr<Node> receiverNode2 = staNodes.Get(2);
        Ipv4Address receiverAddress2 = staInterfaces.GetAddress(2); // IP address of STA3

        OnOffHelper onoff2("ns3::UdpSocketFactory",
                           InetSocketAddress(receiverAddress2, port2));
        onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always ON
        onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never OFF
        ApplicationContainer clientApps2 = onoff2.Install(senderNode2);
        clientApps2.Start(Seconds(1.0)); // Start after 1s
        clientApps2.Stop(Seconds(simulationTime - 1.0)); // Stop 1s before simulation end

        PacketSinkHelper sink2("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port2));
        ApplicationContainer serverApps2 = sink2.Install(receiverNode2);
        serverApps2.Start(Seconds(0.0));
        serverApps2.Stop(Seconds(simulationTime));
    }
    else
    {
        NS_LOG_WARN("Skipping STA2->STA3 application: Not enough STA nodes (nSta < 3).");
    }

    // 11. Tracing
    // Enable promiscuous Pcap tracing on all Wi-Fi devices
    // This generates .pcap files (e.g., wifi-simulation-0-0.pcap for AP, etc.)
    phyHelper.EnablePcapAll(tracingPath, true); 

    // 12. Flow Monitor for performance metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll(); // Install on all nodes

    // 13. Simulation Execution
    Simulator::Stop(Seconds(simulationTime)); // Set the total simulation duration
    Simulator::Run(); // Run the simulation

    // 14. Performance Metrics (Throughput and Packet Loss)
    monitor->CheckForErrors(); // Check if any errors occurred during flow monitoring

    // Get flow statistics and classifier
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    double totalAggregateThroughputMbps = 0;
    uint64_t totalLostPackets = 0;
    uint64_t totalReceivedPackets = 0;
    uint64_t totalSentPackets = 0;

    // Calculate the effective duration for throughput calculation (when applications are active)
    double appActiveDuration = (simulationTime - 1.0) - 1.0; 

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        NS_LOG_INFO("--- Flow ID: " << i->first << " ---");
        NS_LOG_INFO("  Src Addr: " << t.sourceAddress << ", Dst Addr: " << t.destinationAddress);
        NS_LOG_INFO("  Src Port: " << t.sourcePort << ", Dst Port: " << t.destinationPort);
        NS_LOG_INFO("  Protocol: " << (uint32_t)t.protocol);
        NS_LOG_INFO("  Tx Packets: " << i->second.txPackets);
        NS_LOG_INFO("  Rx Packets: " << i->second.rxPackets);
        NS_LOG_INFO("  Lost Packets: " << i->second.lostPackets);
        NS_LOG_INFO("  Rx Bytes: " << i->second.rxBytes);

        // Throughput calculation: (Bytes * 8 bits/Byte) / (Duration in seconds * 1,000,000 bits/Mbps)
        double flowThroughputMbps = (double)i->second.rxBytes * 8 / appActiveDuration / 1000000.0;
        NS_LOG_INFO("  Throughput: " << flowThroughputMbps << " Mbps");
        
        // Average Delay calculation
        if (i->second.rxPackets > 0)
        {
            NS_LOG_INFO("  Avg Delay: " << (i->second.delaySum.GetSeconds() / i->second.rxPackets) * 1000.0 << " ms");
        }
        else
        {
            NS_LOG_INFO("  Avg Delay: N/A (no packets received for this flow)");
        }
        
        totalAggregateThroughputMbps += flowThroughputMbps;
        totalLostPackets += i->second.lostPackets;
        totalReceivedPackets += i->second.rxPackets;
        totalSentPackets += i->second.txPackets;
    }

    NS_LOG_INFO("--- Aggregate Simulation Results ---");
    NS_LOG_INFO("Total Aggregate Throughput (sum of all flows): " << totalAggregateThroughputMbps << " Mbps");
    NS_LOG_INFO("Total Sent Packets: " << totalSentPackets);
    NS_LOG_INFO("Total Received Packets: " << totalReceivedPackets);
    NS_LOG_INFO("Total Lost Packets: " << totalLostPackets);
    if (totalSentPackets > 0)
    {
        double packetLossRate = (double)totalLostPackets / totalSentPackets * 100.0;
        NS_LOG_INFO("Total Packet Loss Rate: " << packetLossRate << " %");
    }
    else
    {
        NS_LOG_INFO("No packets were sent during the simulation.");
    }

    // 15. Cleanup
    Simulator::Destroy(); // Clean up resources
    NS_LOG_INFO("Simulation finished.");

    return 0;
}