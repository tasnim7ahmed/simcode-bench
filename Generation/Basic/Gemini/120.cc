#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <string>
#include <cmath> // For std::ceil

// Define the log component for this simulation script
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocSimulation");

int main (int argc, char *argv[])
{
    // Enable logging for specific components for better debug information
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("AodvAdhocSimulation", LOG_LEVEL_INFO);
    // Optionally enable AODV logging for routing specifics
    // LogComponentEnable ("ns3::AodvRoutingProtocol", LOG_LEVEL_ALL);

    uint32_t numNodes = 10;
    double simulationTime = 60.0; // seconds
    double nodeGridDelta = 30.0;  // meters separation for initial grid layout

    // Allow users to override default values via command-line arguments
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes in the simulation", numNodes);
    cmd.AddValue ("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue ("nodeGridDelta", "Initial grid spacing between nodes in meters", nodeGridDelta);
    cmd.Parse (argc, argv);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create (numNodes);

    // 2. Configure mobility: Grid layout and random mobility
    MobilityHelper mobility;

    // Calculate grid dimensions for numNodes (e.g., 10 nodes -> 5 columns, 2 rows)
    uint32_t gridWidth = 5; // Fixed number of columns for the grid
    uint32_t gridHeight = std::ceil(static_cast<double>(numNodes) / gridWidth); // Calculated rows

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (nodeGridDelta),
                                   "DeltaY", DoubleValue (nodeGridDelta),
                                   "GridWidth", UintegerValue (gridWidth),
                                   "LayoutType", StringValue ("RowFirst"));

    // Random mobility within a defined rectangular area
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (-200, 200, -200, 200)), // x from -200 to 200, y from -200 to 200
                               "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"), // Speed range 1-5 m/s
                               "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]")); // Pause for 0.5 seconds
    mobility.Install (nodes);

    // 3. Configure Wi-Fi for ad-hoc network
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211a); // Use 802.11a standard

    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO); // Required for PCAP tracing

    YansWifiChannelHelper channel;
    // Friis propagation loss model for free space, 5 GHz frequency
    channel.SetPropagationLossModel ("ns3::FriisPropagationLossModel",
                                     "Frequency", DoubleValue (5.15e9));
    channel.SetPropagationDelayModel ("ns3::ConstantSpeedPropagationDelayModel");
    phy.SetChannel (channel.Create ());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
    mac.SetType ("ns3::AdhocWifiMac"); // Set MAC to ad-hoc mode
    NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

    // 4. Configure AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (aodv); // Install AODV as the routing protocol
    internet.Install (nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0"); // 10.0.0.x subnet
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // 6. Implement UDP echo server on each node
    UdpEchoServerHelper echoServer (9); // Listen on port 9
    ApplicationContainer serverApps = echoServer.Install (nodes);
    serverApps.Start (Seconds (1.0)); // Start servers at 1 second
    serverApps.Stop (Seconds (simulationTime - 1.0)); // Stop before simulation ends

    // 7. Implement UDP echo clients for circular communication
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> clientNode = nodes.Get (i);
        // Client i sends to node (i+1) % numNodes
        Ipv4Address serverAddress = interfaces.GetAddress ((i + 1) % numNodes);

        UdpEchoClientHelper echoClient (serverAddress, 9);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1000)); // Send up to 1000 packets
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (4.0))); // Send every 4 seconds
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024)); // 1024 bytes (1KB) packet size

        ApplicationContainer clientApps = echoClient.Install (clientNode);
        clientApps.Start (Seconds (2.0 + i * 0.01)); // Stagger client start times slightly
        clientApps.Stop (Seconds (simulationTime - 1.0));
    }

    // 8. Enable PCAP tracing
    phy.EnablePcapAll ("aodv-adhoc"); // Generates .pcap files for all devices

    // 9. Optional printing of routing tables at specified intervals
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("aodv-routing-table.routes", std::ios::out);
    aodv.PrintRoutingTableAt (Seconds (5.0), nodes, routingStream);
    aodv.PrintRoutingTableAt (Seconds (15.0), nodes, routingStream);
    aodv.PrintRoutingTableAt (Seconds (30.0), nodes, routingStream);
    aodv.PrintRoutingTableAt (Seconds (simulationTime - 2.0), nodes, routingStream);

    // 10. Integrate Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    // 11. Create animation file for NetAnim
    AnimationInterface anim ("aodv-animation.xml");
    // Enable packet metadata for detailed information in NetAnim
    anim.EnablePacketMetadata ();
    // Enable IPv4 route tracking to visualize AODV routing updates
    anim.EnableIpv4RouteTracking ("aodv-routing-animation.xml", Seconds (0), Seconds (simulationTime), Seconds (0.25));

    // Run simulation
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();

    // 12. Capture and output Flow Monitor statistics
    monitor->CheckForExternalEvents (); // Ensure all flows are recorded up to the end of simulation
    flowmon.SerializeToXmlFile ("aodv-flowmon.xml", true, true); // Output XML for detailed analysis

    // Process and print aggregate flow monitor statistics
    double averageThroughputMbps = 0;
    double averagePdr = 0;
    double averageDelaySeconds = 0;
    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalLostPackets = 0;
    uint64_t totalRxBytes = 0;
    double sumOfDelays = 0;
    uint32_t totalReceivedPacketsForDelay = 0;
    uint32_t numActiveFlows = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.Get.Get (0)));
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        NS_LOG_INFO ("Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
        NS_LOG_INFO ("  Tx Packets: " << i->second.txPackets);
        NS_LOG_INFO ("  Rx Packets: " << i->second.rxPackets);
        NS_LOG_INFO ("  Lost Packets: " << i->second.lostPackets);
        NS_LOG_INFO ("  Tx Bytes: " << i->second.txBytes);
        NS_LOG_INFO ("  Rx Bytes: " << i->second.rxBytes);
        NS_LOG_INFO ("  Delay Sum: " << i->second.delaySum);

        if (i->second.txPackets > 0)
        {
            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;
            totalLostPackets += i->second.lostPackets;
            totalRxBytes += i->second.rxBytes;

            // Calculate throughput for this flow
            double flowDuration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
            if (flowDuration > 0)
            {
                double throughput = (i->second.rxBytes * 8.0) / flowDuration / 1024 / 1024; // Mbps
                NS_LOG_INFO ("  Throughput: " << throughput << " Mbps");
            }
            else
            {
                NS_LOG_INFO ("  Throughput: 0 Mbps (Flow duration too short or no packets received)");
            }

            // Calculate Packet Delivery Ratio (PDR) for this flow
            double pdr = (double)i->second.rxPackets / i->second.txPackets * 100.0;
            NS_LOG_INFO ("  Packet Delivery Ratio: " << pdr << " %");

            // Calculate Average End-to-End Delay for this flow
            if (i->second.rxPackets > 0)
            {
                double delay = i->second.delaySum.GetSeconds() / i->second.rxPackets;
                NS_LOG_INFO ("  Avg Delay: " << delay << " s");
                sumOfDelays += i->second.delaySum.GetSeconds();
                totalReceivedPacketsForDelay += i->second.rxPackets;
            }
            else
            {
                NS_LOG_INFO ("  Avg Delay: N/A (No packets received)");
            }
            numActiveFlows++;
        }
    }

    if (numActiveFlows > 0)
    {
        averageThroughputMbps = (totalRxBytes * 8.0) / simulationTime / 1024 / 1024; // Aggregate throughput over simulation time
        averagePdr = (double)totalRxPackets / totalTxPackets * 100.0;
        averageDelaySeconds = sumOfDelays / totalReceivedPacketsForDelay;

        NS_LOG_INFO ("\n--- Aggregate Flow Monitor Statistics ---");
        NS_LOG_INFO ("Total Tx Packets: " << totalTxPackets);
        NS_LOG_INFO ("Total Rx Packets: " << totalRxPackets);
        NS_LOG_INFO ("Total Lost Packets: " << totalLostPackets);
        NS_LOG_INFO ("Total Rx Bytes: " << totalRxBytes);
        NS_LOG_INFO ("Aggregate Throughput: " << averageThroughputMbps << " Mbps");
        NS_LOG_INFO ("Aggregate Packet Delivery Ratio: " << averagePdr << " %");
        NS_LOG_INFO ("Aggregate End-to-End Delay: " << averageDelaySeconds << " s");
        NS_LOG_INFO ("---------------------------------------");
    }
    else
    {
        NS_LOG_WARN ("No active flows recorded by FlowMonitor.");
    }

    Simulator::Destroy ();

    return 0;
}