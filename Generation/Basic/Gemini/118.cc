#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <iomanip>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocGridRingUDP");

int main(int argc, char *argv[])
{
    // Simulation parameters
    double simTime = 20.0; // seconds
    uint32_t numNodes = 4; // Fixed to 4 as per requirement for ring topology
    uint32_t payloadSize = 1024; // bytes
    uint32_t maxPackets = 20;
    double packetInterval = 0.5; // seconds

    // Command line argument parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue("payloadSize", "Payload size of UDP echo packets", payloadSize);
    cmd.AddValue("maxPackets", "Maximum number of packets to send from each client", maxPackets);
    cmd.AddValue("packetInterval", "Time interval between packets in seconds", packetInterval);
    cmd.Parse(argc, argv);

    // Node Creation
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wi-Fi Configuration (Ad hoc)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a); // Using 802.11a (5 GHz band)

    // Set a Remote Station Manager (e.g., AARF) for better performance
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiPhyHelper phy;
    phy.SetChannel(YansWifiChannelHelper::Create()); // Create a channel

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // Ad hoc mode

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility Model (Grid + RandomWalk)
    // First, install GridMobilityModel to set initial grid positions
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::GridMobilityModel",
                              "GridWidth", DoubleValue(2.0), // For 4 nodes, a 2x2 grid
                              "LayoutType", StringValue("RowFirst"),
                              "DeltaX", DoubleValue(50.0), // 50m separation in X
                              "DeltaY", DoubleValue(50.0)); // 50m separation in Y
    mobility.Install(nodes);

    // Store the initial positions set by GridMobilityModel
    std::vector<Vector> initialPositions(numNodes);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<MobilityModel> mm = nodes.Get(i)->GetObject<MobilityModel>();
        initialPositions[i] = mm->GetPosition();
    }

    // Now, replace the mobility model with RandomWalk2dMobilityModel and set its initial position
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)), // Rectangular area for movement
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10.0]"), // Constant speed of 10 m/s
                              "Distance", StringValue("ns3::ConstantRandomVariable[Constant=50.0]")); // Move 50m before changing direction
    mobility.Install(nodes); // This will replace the GridMobilityModel instances

    // Set the initial position for each RandomWalk2dMobilityModel instance to its grid start
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<MobilityModel> mm = nodes.Get(i)->GetObject<MobilityModel>();
        Ptr<RandomWalk2dMobilityModel> rw2d = DynamicCast<RandomWalk2dMobilityModel>(mm);
        if (rw2d)
        {
            rw2d->SetPosition(initialPositions[i]);
        }
        else
        {
            NS_LOG_ERROR("Failed to cast mobility model to RandomWalk2dMobilityModel for node " << i);
        }
    }

    // Internet Stack and IP Addressing
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Applications: UDP Echo Server
    UdpEchoServerHelper echoServer(9); // Port 9 (standard echo port)
    ApplicationContainer serverApps = echoServer.Install(nodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Applications: UDP Echo Clients in Ring Topology
    // Node 0 sends to Node 1, Node 1 to Node 2, Node 2 to Node 3, Node 3 to Node 0
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> senderNode = nodes.Get(i);
        // The receiver is the next node in the ring (circularly)
        Ipv4Address receiverIp = interfaces.GetAddress((i + 1) % numNodes);

        UdpEchoClientHelper echoClient(receiverIp, 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer clientApp = echoClient.Install(senderNode);
        // Stagger client start times slightly to differentiate flows in Flow Monitor
        clientApp.Start(Seconds(1.0 + (double)i * 0.1));
        clientApp.Stop(Seconds(simTime)); // Clients run for the entire simulation duration
    }

    // Flow Monitor Setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim Animation Setup
    NetAnimHelper anim;
    anim.SetOutputFileName("adhoc-grid-ring-animation.xml");
    anim.EnablePacketTracking();
    anim.EnableIpv4RouteTracking(); // Enable IPv4 route tracking for animation
    anim.EnableWifiMacTracking();   // Enable Wi-Fi MAC tracking for animation
    anim.EnableAnimation();

    // Simulation Run
    NS_LOG_INFO("Running simulation for " << simTime << " seconds...");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow Monitor Statistics Output
    monitor->CheckForDups(); // Check for duplicate packets

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalTxBytes = 0;
    double totalRxBytes = 0;
    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalLostPackets = 0; // Lost packets reported by FlowMonitor (Tx - Rx, considering duplicates)
    double totalDelaySum = 0;
    uint32_t delayCount = 0; // Number of packets contributing to delaySum

    std::cout << "\n--- Flow Monitor Statistics ---" << std::endl;
    std::cout << std::fixed << std::setprecision(3); // Set precision for output

    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl; // FlowMonitor reports lost packets
        std::cout << "  Tx Bytes: " << iter->second.txBytes << std::endl;
        std::cout << "  Rx Bytes: " << iter->second.rxBytes << std::endl;

        if (iter->second.rxPackets > 0)
        {
            double throughput = iter->second.rxBytes * 8.0 / (simTime * 1000 * 1000); // Mbps
            double pdr = (double)iter->second.rxPackets / iter->second.txPackets * 100.0; // Percentage
            double avgDelay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets; // Seconds
            double avgJitter = iter->second.jitterSum.GetSeconds() / iter->second.rxPackets; // Seconds

            std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
            std::cout << "  Packet Delivery Ratio: " << pdr << " %" << std::endl;
            std::cout << "  Average Delay: " << avgDelay << " s" << std::endl;
            std::cout << "  Jitter: " << avgJitter << " s" << std::endl;
        }
        else
        {
            std::cout << "  Throughput: 0 Mbps" << std::endl;
            std::cout << "  Packet Delivery Ratio: 0 %" << std::endl;
            std::cout << "  Average Delay: N/A" << std::endl;
            std::cout << "  Jitter: N/A" << std::endl;
        }
        std::cout << std::endl;

        totalTxBytes += iter->second.txBytes;
        totalRxBytes += iter->second.rxBytes;
        totalTxPackets += iter->second.txPackets;
        totalRxPackets += iter->second.rxPackets;
        totalLostPackets += iter->second.lostPackets;

        if (iter->second.rxPackets > 0)
        {
            totalDelaySum += iter->second.delaySum.GetSeconds();
            delayCount += iter->second.rxPackets;
        }
    }

    std::cout << "--- Overall Network Statistics ---" << std::endl;
    std::cout << "Total Tx Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
    std::cout << "Total Lost Packets (from FlowMon): " << totalLostPackets << std::endl;
    std::cout << "Total Tx Bytes: " << totalTxBytes << std::endl;
    std::cout << "Total Rx Bytes: " << totalRxBytes << std::endl;

    if (totalTxPackets > 0)
    {
        std::cout << "Overall Packet Delivery Ratio: " << (double)totalRxPackets / totalTxPackets * 100.0 << " %" << std::endl;
    }
    else
    {
        std::cout << "Overall Packet Delivery Ratio: N/A (No packets sent)" << std::endl;
    }

    if (simTime > 0)
    {
        std::cout << "Overall Throughput: " << totalRxBytes * 8.0 / (simTime * 1000 * 1000) << " Mbps" << std::endl;
    }
    else
    {
        std::cout << "Overall Throughput: N/A (Simulation time is zero)" << std::endl;
    }

    if (delayCount > 0)
    {
        std::cout << "Overall Average End-to-End Delay: " << totalDelaySum / delayCount << " s" << std::endl;
    }
    else
    {
        std::cout << "Overall Average End-to-End Delay: N/A (No packets received)" << std::endl;
    }
    std::cout << "---------------------------------" << std::endl;

    // Clean up simulation resources
    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    return 0;
}