#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/aodv-module.h"

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    // 1. Set up simulation parameters
    double simDuration = 10.0; // seconds
    uint32_t starClients = 3;  // Number of clients in star topology (excluding server)
    uint32_t meshNodes = 4;    // Number of nodes in mesh topology
    uint32_t udpPacketSize = 1024; // bytes for UDP
    std::string udpDataRate = "1Mbps"; // Data rate for UDP OnOff applications

    CommandLine cmd(__FILE__);
    cmd.AddValue("simDuration", "Simulation duration in seconds", simDuration);
    cmd.AddValue("starClients", "Number of clients in star topology (excluding server)", starClients);
    cmd.AddValue("meshNodes", "Number of nodes in mesh topology", meshNodes);
    cmd.AddValue("udpPacketSize", "Packet size in bytes for UDP applications", udpPacketSize);
    cmd.AddValue("udpDataRate", "Data rate for UDP OnOff applications (e.g., 1Mbps)", udpDataRate);
    cmd.Parse(argc, argv);

    uint32_t totalStarNodes = starClients + 1; // Clients + 1 server

    // 2. Enable logging
    LogComponentEnable("HybridNetworkSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO); // Log AODV routing events

    // 3. Create nodes
    NS_LOG_INFO("Creating nodes...");
    NodeContainer starNodes;
    starNodes.Create(totalStarNodes); // Node 0 is server, others are clients

    NodeContainer meshNodesContainer;
    meshNodesContainer.Create(meshNodes);

    // 4. Install Internet Stack
    NS_LOG_INFO("Installing Internet Stack...");
    InternetStackHelper internet;
    internet.Install(starNodes);

    // Install AODV routing protocol for mesh nodes
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(meshNodesContainer); // Mesh nodes use AODV

    // 5. Create channels and install network devices
    // 5.1. Star Topology (CSMA)
    NS_LOG_INFO("Setting up Star Topology (CSMA)...");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer starDevices = csma.Install(starNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer starInterfaces = ipv4.Assign(starDevices);

    // 5.2. Mesh Topology (WiFi Ad-Hoc)
    NS_LOG_INFO("Setting up Mesh Topology (WiFi Ad-Hoc)...");
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataRate", StringValue("6Mbps")); // Fixed data rate for simplicity

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel"); // Simple propagation loss model
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer meshDevices = wifi.Install(wifiPhy, wifiMac, meshNodesContainer);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = ipv4.Assign(meshDevices);

    // 6. Set up Mobility (for Mesh nodes)
    NS_LOG_INFO("Setting up Mobility for Mesh Nodes...");
    MobilityHelper mobility;
    // Position mesh nodes in a grid for clear visualization
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0), // Spacing between nodes
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(2), // 2 nodes per row
                                  "LayoutType", StringValue("ns3::GridPositionAllocator::ROW_FIRST"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // Nodes remain stationary
    mobility.Install(meshNodesContainer);

    // 7. Install Applications

    // 7.1. TCP Star Applications
    NS_LOG_INFO("Setting up TCP Star Applications...");
    // Server: Node 0 of starNodes
    Ptr<Node> starServer = starNodes.Get(0);
    uint16_t tcpPort = 9000;
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer serverApps = tcpSinkHelper.Install(starServer);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration + 1)); // Stop after simulation ends to capture all data

    // Clients: Node 1 to totalStarNodes-1 of starNodes
    BulkSendHelper tcpClientHelper("ns3::TcpSocketFactory", InetSocketAddress(starInterfaces.GetAddress(0), tcpPort));
    tcpClientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send until stop time
    tcpClientHelper.SetAttribute("SendBufferSize", UintegerValue(131072)); // 128KB buffer for TCP

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < totalStarNodes; ++i) {
        clientApps.Add(tcpClientHelper.Install(starNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0)); // Start client flows at 1 second
    clientApps.Stop(Seconds(simDuration));

    // 7.2. UDP Mesh Applications
    NS_LOG_INFO("Setting up UDP Mesh Applications...");
    uint16_t udpPort = 8000;

    // Install PacketSinks on all mesh nodes to receive UDP traffic
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApps = udpSinkHelper.Install(meshNodesContainer);
    udpSinkApps.Start(Seconds(0.0));
    udpSinkApps.Stop(Seconds(simDuration + 1));

    // Install OnOff applications (UDP sources)
    OnOffHelper onoffHelper("ns3::UdpSocketFactory", Address());
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Always On
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate(udpDataRate)));
    onoffHelper.SetAttribute("PacketSize", UintegerValue(udpPacketSize));

    ApplicationContainer udpSourceApps;
    // Example: Each node sends to the next node in a ring fashion, and one cross-flow
    for (uint32_t i = 0; i < meshNodes; ++i) {
        Ptr<Node> sender = meshNodesContainer.Get(i);
        Ipv4Address receiverAddress = meshInterfaces.GetAddress((i + 1) % meshNodes); // Send to next node in ring

        InetSocketAddress remoteAddress(receiverAddress, udpPort);
        onoffHelper.SetAttribute("Remote", AddressValue(remoteAddress));
        udpSourceApps.Add(onoffHelper.Install(sender));
    }
    // Add one extra cross-flow, e.g., Node 0 to Node 2 (if enough nodes)
    if (meshNodes >= 3) {
        Ptr<Node> sender = meshNodesContainer.Get(0);
        Ipv4Address receiverAddress = meshInterfaces.GetAddress(2);
        InetSocketAddress remoteAddress(receiverAddress, udpPort);
        onoffHelper.SetAttribute("Remote", AddressValue(remoteAddress));
        udpSourceApps.Add(onoffHelper.Install(sender));
    }
    udpSourceApps.Start(Seconds(1.5)); // Start UDP flows slightly after TCP
    udpSourceApps.Stop(Seconds(simDuration));

    // 8. Configure Tracing
    NS_LOG_INFO("Configuring Tracing...");
    // PCAP tracing
    csma.EnablePcapAll("star-network-pcap");
    wifiPhy.EnablePcapAll("mesh-network-pcap");

    // FlowMonitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // NetAnim
    AnimationInterface anim("simulation.xml");
    // Set fixed positions for star nodes for clear visualization
    // Star nodes are placed to the right of mesh nodes
    for (uint32_t i = 0; i < totalStarNodes; ++i) {
        anim.SetConstantPosition(starNodes.Get(i), 100.0, 10.0 + i * 15.0);
    }
    // Mesh node positions are set by mobility model automatically (relative to origin (0,0))
    // We can change default colors for visual distinction
    anim.UpdateNodeColor(starNodes.Get(0), 255, 0, 0); // Red for TCP server
    anim.UpdateNodeDescription(starNodes.Get(0), "TCP_Server");
    for (uint32_t i = 1; i < totalStarNodes; ++i) {
        anim.UpdateNodeColor(starNodes.Get(i), 0, 0, 255); // Blue for TCP clients
        anim.UpdateNodeDescription(starNodes.Get(i), "TCP_Client");
    }
    for (uint32_t i = 0; i < meshNodes; ++i) {
        anim.UpdateNodeColor(meshNodesContainer.Get(i), 0, 255, 0); // Green for UDP mesh nodes
        anim.UpdateNodeDescription(meshNodesContainer.Get(i), "UDP_Mesh_Node");
    }

    // 9. Run simulation
    NS_LOG_INFO("Running Simulation...");
    Simulator::Stop(Seconds(simDuration + 2)); // Ensure FlowMonitor captures everything
    Simulator::Run();

    // 10. Analyze FlowMonitor results
    NS_LOG_INFO("Analyzing FlowMonitor results...");
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(monitor->GetFlowClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalTcpThroughputMbps = 0;
    double totalUdpThroughputMbps = 0;
    uint32_t totalTcpRxPackets = 0;
    uint32_t totalUdpRxPackets = 0;
    uint32_t totalTcpTxPackets = 0;
    uint32_t totalUdpTxPackets = 0;
    uint32_t totalTcpLostPackets = 0;
    uint32_t totalUdpLostPackets = 0;
    Time totalTcpDelaySum;
    Time totalUdpDelaySum;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        
        // Calculate flow duration from first Tx to last Rx, or use simulation duration if no valid Rx
        double flowDuration = it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds();
        if (flowDuration <= 0) { 
            flowDuration = simDuration; 
        }
        
        double throughputMbps = (double)it->second.rxBytes * 8.0 / (flowDuration * 1000000.0);
        double avgDelaySec = (it->second.rxPackets > 0) ? it->second.delaySum.GetSeconds() / it->second.rxPackets : 0.0;
        double jitterSec = (it->second.rxPackets > 0) ? it->second.jitterSum.GetSeconds() / it->second.rxPackets : 0.0;
        double packetLossRate = (it->second.txPackets > 0) ? (double)it->second.lostPackets / it->second.txPackets * 100.0 : 0.0;

        std::cout << "Flow ID: " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") "
                  << "Protocol: " << (t.protocol == 6 ? "TCP" : (t.protocol == 17 ? "UDP" : "Other")) << std::endl;
        std::cout << "  Tx Bytes: " << it->second.txBytes << std::endl;
        std::cout << "  Rx Bytes: " << it->second.rxBytes << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << throughputMbps << " Mbps" << std::endl;
        std::cout << "  Average Delay: " << avgDelaySec << " s" << std::endl;
        std::cout << "  Jitter: " << jitterSec << " s" << std::endl;
        std::cout << "  Packet Loss Rate: " << packetLossRate << " %" << std::endl;

        if (t.protocol == 6) { // TCP
            totalTcpThroughputMbps += throughputMbps;
            totalTcpRxPackets += it->second.rxPackets;
            totalTcpTxPackets += it->second.txPackets;
            totalTcpLostPackets += it->second.lostPackets;
            totalTcpDelaySum += it->second.delaySum;
        } else if (t.protocol == 17) { // UDP
            totalUdpThroughputMbps += throughputMbps;
            totalUdpRxPackets += it->second.rxPackets;
            totalUdpTxPackets += it->second.txPackets;
            totalUdpLostPackets += it->second.lostPackets;
            totalUdpDelaySum += it->second.delaySum;
        }
    }

    std::cout << "\n===================================" << std::endl;
    std::cout << "--- Aggregate TCP Metrics ---" << std::endl;
    std::cout << "Total TCP Throughput: " << totalTcpThroughputMbps << " Mbps" << std::endl;
    std::cout << "Total TCP Rx Packets: " << totalTcpRxPackets << std::endl;
    std::cout << "Total TCP Tx Packets: " << totalTcpTxPackets << std::endl;
    std::cout << "Total TCP Lost Packets: " << totalTcpLostPackets << std::endl;
    if (totalTcpTxPackets > 0) {
        std::cout << "Total TCP Packet Loss Rate: " << (double)totalTcpLostPackets / totalTcpTxPackets * 100.0 << " %" << std::endl;
    }
    if (totalTcpRxPackets > 0) {
        std::cout << "Total TCP Average Delay: " << totalTcpDelaySum.GetSeconds() / totalTcpRxPackets << " s" << std::endl;
    }

    std::cout << "\n--- Aggregate UDP Metrics ---" << std::endl;
    std::cout << "Total UDP Throughput: " << totalUdpThroughputMbps << " Mbps" << std::endl;
    std::cout << "Total UDP Rx Packets: " << totalUdpRxPackets << std::endl;
    std::cout << "Total UDP Tx Packets: " << totalUdpTxPackets << std::endl;
    std::cout << "Total UDP Lost Packets: " << totalUdpLostPackets << std::endl;
    if (totalUdpTxPackets > 0) {
        std::cout << "Total UDP Packet Loss Rate: " << (double)totalUdpLostPackets / totalUdpTxPackets * 100.0 << " %" << std::endl;
    }
    if (totalUdpRxPackets > 0) {
        std::cout << "Total UDP Average Delay: " << totalUdpDelaySum.GetSeconds() / totalUdpRxPackets << " s" << std::endl;
    }
    std::cout << "===================================" << std::endl;

    // 11. Cleanup
    NS_LOG_INFO("Destroying Simulation...");
    Simulator::Destroy();

    return 0;
}