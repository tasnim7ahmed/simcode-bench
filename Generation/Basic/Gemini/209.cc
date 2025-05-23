#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-module.h"

#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // For std::fixed and std::setprecision
#include <cmath>   // For M_PI, cos, sin

// Use the ns3 namespace for convenience
using namespace ns3;

// Define a logging component for this simulation script
NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char* argv[])
{
    // 1. Set up default simulation parameters
    uint32_t numClients = 5;
    double simulationTime = 10.0; // seconds
    std::string dataRate = "10Mbps";
    std::string delay = "2ms";
    uint32_t serverPort = 9; // Echo port is commonly used for PacketSink
    uint32_t clientPacketSize = 1024; // bytes per packet
    uint32_t numPacketsToSend = 1000; // Number of packets each client sends

    // 2. Allow parameters to be configured from the command line
    CommandLine cmd(__FILE__);
    cmd.AddValue("numClients", "Number of client nodes (excluding server)", numClients);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Point-to-point link data rate", dataRate);
    cmd.AddValue("delay", "Point-to-point link delay", delay);
    cmd.AddValue("serverPort", "TCP server port number", serverPort);
    cmd.AddValue("clientPacketSize", "Size of packets sent by clients (bytes)", clientPacketSize);
    cmd.AddValue("numPacketsToSend", "Number of packets each client sends", numPacketsToSend);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating " << numClients << " clients and 1 central server.");

    // 3. Create Nodes
    NodeContainer serverNode;
    serverNode.Create(1);
    NodeContainer clientNodes;
    clientNodes.Create(numClients);

    // 4. Install Internet Stack (TCP/IP protocols) on all nodes
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(clientNodes);

    // 5. Configure Point-to-Point Helper for links
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2pHelper.SetChannelAttribute("Delay", StringValue(delay));

    // 6. Connect Nodes and Assign IP Addresses for the Star Topology
    // The server will have a separate IP address for each link connecting to a client.
    std::vector<Ipv4Address> serverIpsOnLinks(numClients); // To store server's IP for each specific link

    Ipv4AddressHelper ipv4;
    std::string subnetBase = "10.1.";

    for (uint32_t i = 0; i < numClients; ++i)
    {
        NS_LOG_INFO("Creating point-to-point link between Server and Client " << i + 1);
        
        // Install devices on the server and the current client node
        NetDeviceContainer linkDevices = p2pHelper.Install(serverNode.Get(0), clientNodes.Get(i));
        
        // Assign IP addresses to this link (e.g., 10.1.1.0/24, 10.1.2.0/24, etc.)
        std::string subnet = subnetBase + std::to_string(i + 1) + ".0";
        ipv4.SetBase(subnet.c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaceContainer = ipv4.Assign(linkDevices);
        
        // Store the server's IP address on this specific link
        serverIpsOnLinks[i] = interfaceContainer.GetAddress(0); // Server's IP on this segment
    }

    // 7. Populate routing tables
    // Global routing will automatically configure routes for the directly connected star topology.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 8. Install Applications (TCP Communication)
    // Server application (PacketSink)
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer serverApps = sinkHelper.Install(serverNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Client applications (BulkSend)
    // Calculate total bytes each client will send
    uint64_t clientMaxBytes = (uint64_t)clientPacketSize * numPacketsToSend;

    for (uint32_t i = 0; i < numClients; ++i)
    {
        NS_LOG_INFO("Setting up BulkSend application for Client " << i + 1);
        // Each client sends data to the server's IP address on its connected link
        BulkSendHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(serverIpsOnLinks[i], serverPort));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(clientMaxBytes));
        clientHelper.SetAttribute("SendSize", UintegerValue(clientPacketSize)); // Size of each TCP segment payload

        ApplicationContainer clientApps = clientHelper.Install(clientNodes.Get(i));
        // Stagger client start times slightly to avoid a single large burst at t=1.0s
        clientApps.Start(Seconds(1.0 + i * 0.1));
        clientApps.Stop(Seconds(simulationTime - 0.1)); // Stop just before simulation ends
    }

    // 9. NetAnim Visualization
    NetAnimHelper netanimHelper;
    netanimHelper.SetOutputFileName("star-topology.xml");

    // Set node positions for a clear star visualization
    // Server in the center
    Ptr<Node> sNode = serverNode.Get(0);
    sNode->AggregateObject(CreateObject<ConstantPositionMobilityModel>());
    sNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // Clients arranged in a circle around the server
    double radius = 50.0;
    for (uint32_t i = 0; i < numClients; ++i)
    {
        Ptr<Node> cNode = clientNodes.Get(i);
        cNode->AggregateObject(CreateObject<ConstantPositionMobilityModel>());
        double angle = i * (2 * M_PI / numClients); // Angle for distributing clients evenly
        cNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(radius * std::cos(angle), radius * std::sin(angle), 0.0));
    }

    netanimHelper.EnablePacketTracking();       // Track packet movement
    netanimHelper.EnableIpv4RouteTracking();    // Track IPv4 routes
    netanimHelper.EnableNodesListRoutingTracking(); // Track node creation/deletion

    netanimHelper.EnableAnimation(); // Generate the XML animation file

    // 10. Flow Monitor for comprehensive metrics measurement
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // 11. Run the Simulation
    NS_LOG_INFO("Running simulation for " << simulationTime << " seconds...");
    Simulator::Stop(Seconds(simulationTime)); // Schedule simulator to stop at specified time
    Simulator::Run();
    NS_LOG_INFO("Simulation finished.");

    // 12. Collect and print results from Flow Monitor
    // Specify which metrics to check for (throughput, latency, packet loss are requested)
    flowMonitor->CheckFor = FlowMonitor::LATENCY | FlowMonitor::THROUGHPUT | FlowMonitor::PACKET_LOSS;
    flowMonitor->SerializeToXmlFile("star-topology-flowmon.xml", true, true); // Save flow stats to XML

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetFlowClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    double totalTxBytes = 0;
    double totalRxBytes = 0;
    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalLostPackets = 0;
    double totalDelaySumSeconds = 0;

    std::cout << "\n=== Flow Monitor Results (Per Flow) ===\n";
    std::cout << std::fixed << std::setprecision(3); // Set precision for floating point output

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> " << t.destinationAddress << ":" << t.destinationPort << ")\n";
        
        totalTxBytes += i->second.txBytes;
        totalRxBytes += i->second.rxBytes;
        totalTxPackets += i->second.txPackets;
        totalRxPackets += i->second.rxPackets;
        totalLostPackets += i->second.lostPackets; // Lost packets include packets dropped by queue/device

        std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";

        if (i->second.rxPackets > 0)
        {
            // Throughput calculation: (Bytes * 8 bits/Byte) / (Time Last Rx - Time First Tx)
            double throughput = (i->second.rxBytes * 8.0) / 
                                (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());
            std::cout << "  Throughput: " << (throughput / 1024 / 1024) << " Mbps\n";

            // Average Latency: Sum of packet delays / Number of received packets
            double avgLatency = i->second.delaySum.GetSeconds() / i->second.rxPackets;
            std::cout << "  Average Latency: " << (avgLatency * 1000) << " ms\n";
            totalDelaySumSeconds += i->second.delaySum.GetSeconds(); // Accumulate for overall average

            // Jitter: Sum of squared differences of consecutive packet delays / (Number of received packets - 1)
            // Note: For jitter, need at least 2 packets.
            if (i->second.rxPackets > 1) {
                double avgJitter = i->second.jitterSum.GetSeconds() / (i->second.rxPackets - 1.0);
                std::cout << "  Jitter: " << (avgJitter * 1000) << " ms\n";
            } else {
                std::cout << "  Jitter: N/A (less than 2 packets received)\n";
            }
        }
        else
        {
            std::cout << "  No packets received for this flow.\n";
            std::cout << "  Throughput: 0.000 Mbps\n";
            std::cout << "  Average Latency: N/A\n";
            std::cout << "  Jitter: N/A\n";
        }
        
        // Packet Loss Rate
        double packetLossRate = (i->second.txPackets > 0) ? (static_cast<double>(i->second.lostPackets) / i->second.txPackets * 100.0) : 0.0;
        std::cout << "  Packet Loss Rate: " << packetLossRate << " %\n";
        std::cout << "------------------------------------------\n";
    }

    // Overall Simulation Metrics
    std::cout << "\n=== Overall Simulation Metrics ===\n";
    std::cout << "Total Tx Bytes: " << totalTxBytes << "\n";
    std::cout << "Total Rx Bytes: " << totalRxBytes << "\n";
    std::cout << "Total Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Total Lost Packets: " << totalLostPackets << "\n";

    double overallThroughput = (totalRxBytes * 8.0) / simulationTime;
    std::cout << "Overall Throughput: " << (overallThroughput / 1024 / 1024) << " Mbps\n";

    if (totalRxPackets > 0)
    {
        double overallAvgLatency = totalDelaySumSeconds / totalRxPackets;
        std::cout << "Overall Average Latency: " << (overallAvgLatency * 1000) << " ms\n";
    }
    else
    {
        std::cout << "Overall Average Latency: N/A (no packets received)\n";
    }
    
    double overallPacketLossRate = (totalTxPackets > 0) ? (static_cast<double>(totalLostPackets) / totalTxPackets * 100.0) : 0.0;
    std::cout << "Overall Packet Loss Rate: " << overallPacketLossRate << " %\n";
    std::cout << "===================================\n";

    // 13. Cleanup simulation resources
    Simulator::Destroy();

    return 0;
}