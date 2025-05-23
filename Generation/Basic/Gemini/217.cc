#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"

#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AODVMobilitySimulation");

// Global file stream for mobility trace
std::ofstream g_mobilityTraceFile;

/**
 * \brief Logs the current position of all nodes to a global file stream.
 * \param nodes The NodeContainer holding all simulation nodes.
 *
 * This function is scheduled periodically to capture mobility trace.
 */
void LogNodePositions(NodeContainer nodes)
{
    g_mobilityTraceFile << "Time: " << Simulator::Now().GetSeconds() << "s\n";
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        if (mobility)
        {
            Vector pos = mobility->GetPosition();
            g_mobilityTraceFile << "  Node " << i << ": x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z << "\n";
        }
    }
    // Reschedule for the next log
    Simulator::Schedule(Seconds(1.0), &LogNodePositions, nodes); 
}

int main(int argc, char *argv[])
{
    // 1. Simulation Parameters - Default values, can be overridden via command line
    uint32_t nNodes = 5;
    double simulationTime = 100.0; // seconds
    double packetInterval = 1.0;   // seconds (for UDP Echo Client)
    uint32_t packetSize = 1024;    // bytes (for UDP Echo Client)
    std::string phyMode = "DsssRate1Mbps"; // Wifi physical mode
    double minSpeed = 5;   // m/s for RandomWalk2dMobilityModel
    double maxSpeed = 10;  // m/s for RandomWalk2dMobilityModel
    double pauseTime = 1;  // s for RandomWalk2dMobilityModel
    
    // Calculate total packets based on simulation time and interval
    uint32_t numPackets = (uint32_t)(simulationTime / packetInterval);

    // Command line argument parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of nodes in the network", nNodes);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("packetInterval", "Time interval between packets sent by the client", packetInterval);
    cmd.AddValue("packetSize", "Size of UDP echo packets in bytes", packetSize);
    cmd.AddValue("phyMode", "Wifi Phy mode (e.g., DsssRate1Mbps, OfdmRate6Mbps)", phyMode);
    cmd.AddValue("minSpeed", "Minimum speed for RandomWalk2dMobilityModel in m/s", minSpeed);
    cmd.AddValue("maxSpeed", "Maximum speed for RandomWalk2dMobilityModel in m/s", maxSpeed);
    cmd.AddValue("pauseTime", "Pause time for RandomWalk2dMobilityModel in seconds", pauseTime);
    cmd.Parse(argc, argv);

    // 2. Configure Logging
    LogComponentEnable("AODVMobilitySimulation", LOG_LEVEL_INFO);
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("WifiPhy", LOG_LEVEL_INFO); // Uncomment for detailed PHY logs

    // Open mobility trace file for writing
    g_mobilityTraceFile.open("aodv-mobility-trace.txt");
    if (!g_mobilityTraceFile.is_open())
    {
        NS_FATAL_ERROR("Failed to open mobility trace file 'aodv-mobility-trace.txt'.");
    }

    // 3. Create Nodes
    NS_LOG_INFO("Creating " << nNodes << " nodes.");
    NodeContainer nodes;
    nodes.Create(nNodes);

    // 4. Configure Mobility Model
    NS_LOG_INFO("Setting up RandomWalk2dMobilityModel.");
    MobilityHelper mobility;
    // Define a rectangular area for nodes to move within
    Ptr<UniformRandomVariable> speedRv = CreateObject<UniformRandomVariable>();
    speedRv->SetAttribute("Min", DoubleValue(minSpeed));
    speedRv->SetAttribute("Max", DoubleValue(maxSpeed));

    Ptr<ConstantRandomVariable> pauseRv = CreateObject<ConstantRandomVariable>();
    pauseRv->SetAttribute("Constant", DoubleValue(pauseTime));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)), // X and Y from -500 to 500
                              "Speed", PointerValue(speedRv),
                              "Pause", PointerValue(pauseRv));
    
    // Set initial positions randomly within a smaller rectangle to avoid starting at boundaries
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=-400|Max=400]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=-400|Max=400]"));
    mobility.Install(nodes);

    // Schedule the periodic logging of node positions for mobility trace
    Simulator::Schedule(Seconds(0.0), &LogNodePositions, nodes);

    // 5. Configure Wifi Devices
    NS_LOG_INFO("Setting up Wifi devices.");
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b); // Use 802.11b standard for Ad-hoc
    wifi.SetRemoteStationManager("ns3::AarfWifiManager"); // AARF for rate adaptation

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc MAC layer for MANET

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", DoubleValue(7.5)); // Set a reasonable transmit power (dBm)
    wifiPhy.Set("TxPowerEnd", DoubleValue(7.5));
    wifiPhy.Set("RxGain", DoubleValue(0)); // Default Rx gain
    wifiPhy.Set("TxGain", DoubleValue(0)); // Default Tx gain
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11);
    
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel"); // Friis propagation loss
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel"); // Constant speed delay
    
    // Create and install the channel and devices
    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 6. Install Internet Stack with AODV Routing
    NS_LOG_INFO("Installing Internet Stack with AODV routing.");
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internet.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 7. Install Applications (UDP Echo Client/Server)
    NS_LOG_INFO("Installing UDP Echo applications.");
    // Server on the last node (e.g., node 4 if nNodes = 5)
    UdpEchoServerHelper echoServer(9); // Listen on port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(nNodes - 1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1.0)); // Run slightly longer than client

    // Client on the first node (node 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(nNodes - 1), 9); // Point to server's IP and port
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start client after server
    clientApps.Stop(Seconds(simulationTime));

    // 8. Configure Tracing
    NS_LOG_INFO("Enabling PCAP and ASCII traces.");
    // PCAP traces for Wifi devices to analyze packets in Wireshark
    wifiPhy.EnablePcap("aodv-mobility", devices);
    // ASCII traces for AODV routing protocol activities (e.g., routing table changes)
    aodv.EnableAsciiAll("aodv-routing", devices.Get(0)); // Trace AODV on all interfaces of node 0

    // 9. Configure Flow Monitor for Packet Delivery Analysis
    NS_LOG_INFO("Setting up Flow Monitor.");
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.Install(nodes);

    // 10. Run Simulation
    NS_LOG_INFO("Running simulation for " << simulationTime << " seconds.");
    Simulator::Stop(Seconds(simulationTime + 5.0)); // Stop a bit after applications finish
    Simulator::Run();

    // 11. Analyze Flow Monitor Statistics
    NS_LOG_INFO("Analyzing Flow Monitor statistics.");
    monitor->CheckForEowingChanges(); // Update statistics before retrieving
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::Stats stats = monitor->GetStats();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalTxBytes = 0;
    double totalRxBytes = 0;
    double totalDelaySum = 0; // Sum of all packet delays
    uint32_t rxPacketsForDelay = 0; // Count of received packets to average delay

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.GetIpv4FlowStats().begin();
         i != stats.GetIpv4FlowStats().end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->GetFiveTuple(i->first);

        // Filter for the specific flow from client (node 0) to server (last node)
        if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(nNodes - 1))
        {
            NS_LOG_INFO("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
            NS_LOG_INFO("  Tx Packets: " << i->second.txPackets);
            NS_LOG_INFO("  Rx Packets: " << i->second.rxPackets);
            NS_LOG_INFO("  Tx Bytes:   " << i->second.txBytes);
            NS_LOG_INFO("  Rx Bytes:   " << i->second.rxBytes);
            
            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;
            totalTxBytes += i->second.txBytes;
            totalRxBytes += i->second.rxBytes;

            // Calculate delay and throughput only if packets were received
            if (i->second.rxPackets > 0)
            {
                totalDelaySum += i->second.delaySum.GetSeconds();
                rxPacketsForDelay += i->second.rxPackets;
                
                double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000.0; // Mbps
                NS_LOG_INFO("  Throughput: " << throughput << " Mbps");
            }
            else
            {
                NS_LOG_INFO("  Throughput: 0 Mbps (No packets received)");
            }
        }
    }

    NS_LOG_INFO("--- AODV Packet Delivery Analysis Summary ---");
    NS_LOG_INFO("Total Transmitted Packets (Client): " << totalTxPackets);
    NS_LOG_INFO("Total Received Packets (Server): " << totalRxPackets);
    NS_LOG_INFO("Total Transmitted Bytes (Client): " << totalTxBytes);
    NS_LOG_INFO("Total Received Bytes (Server): " << totalRxBytes);

    if (totalTxPackets > 0)
    {
        double pdr = (double)totalRxPackets / totalTxPackets;
        NS_LOG_INFO("Packet Delivery Ratio (PDR): " << pdr * 100.0 << "%");
    }
    else
    {
        NS_LOG_INFO("No packets transmitted by the client to analyze PDR.");
    }

    if (rxPacketsForDelay > 0)
    {
        double avgDelay = totalDelaySum / rxPacketsForDelay;
        NS_LOG_INFO("Average Packet Delay: " << avgDelay << " seconds");
    }
    else
    {
        NS_LOG_INFO("No packets received for average delay calculation.");
    }
    NS_LOG_INFO("----------------------------------------------");

    // 12. Cleanup
    Simulator::Destroy();
    g_mobilityTraceFile.close(); // Close the mobility trace file

    return 0;
}