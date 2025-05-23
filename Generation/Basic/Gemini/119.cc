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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocGridWifi");

int main(int argc, char *argv[]) {
    // Set up default values for simulation parameters
    double simulationTime = 20.0; // seconds
    uint32_t numNodes = 6;
    double nodeGridDeltaX = 50.0; // meters separation between nodes in grid
    double nodeGridDeltaY = 50.0; // meters separation between nodes in grid
    uint32_t gridWidth = 3;       // For 6 nodes, this will create a 3x2 grid
    double mobilityBoundsMinX = -100.0; // X-axis lower bound for random movement
    double mobilityBoundsMaxX = 100.0;  // X-axis upper bound for random movement
    double mobilityBoundsMinY = -100.0; // Y-axis lower bound for random movement
    double mobilityBoundsMaxY = 100.0;  // Y-axis upper bound for random movement
    double randomWalkDistance = 20.0;   // meters per step for random walk
    double randomWalkSpeed = 1.0;       // m/s for random walk

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Enable logging for relevant components
    LogComponentEnable("AdHocGridWifi", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // 1. Create six nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 2. Configure Ad Hoc Wireless Network (IEEE 802.11)
    WifiHelper wifi;
    wifi.SetStandard(WifiStandard::k80211a); // Using 802.11a standard

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel;
    // Use NistPropagationLossModel for a realistic outdoor propagation
    channel.SetPropagationLossModel("ns3::NistPropagationLossModel");
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    phy.SetChannel(channel.Create());

    // Set up Ad Hoc MAC layer
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Disable fragmentation and RTS/CTS for simplicity in ad-hoc scenarios
    Config::Set("/NodeList/*/DeviceList/*/Mac/FragmentationThreshold", StringValue("2200"));
    Config::Set("/NodeList/*/DeviceList/*/Mac/RtsCtsThreshold", StringValue("2200"));

    // 3. Implement a Grid Mobility Model with Random Movement
    MobilityHelper mobility;
    // Initial grid layout
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(nodeGridDeltaX),
                                  "DeltaY", DoubleValue(nodeGridDeltaY),
                                  "GridWidth", UintegerValue(gridWidth),
                                  "LayoutType", StringValue("RowFirst"));

    // Random movement within a specified rectangular area
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(mobilityBoundsMinX, mobilityBoundsMaxX, mobilityBoundsMinY, mobilityBoundsMaxY)),
                              "Distance", DoubleValue(randomWalkDistance),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(randomWalkSpeed) + "]"));
    mobility.Install(nodes);

    // 4. Install the Internet stack on each node
    InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IPv4 addresses from the 192.9.39.0/24 subnet
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Establish UDP echo server applications on each node
    // Port 9 is often used for discard/echo
    UdpEchoServerHelper echoServer(9); 
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0)); // Servers start early
    serverApps.Stop(Seconds(simulationTime - 1.0));

    // Configure UDP echo client applications in a ring topology
    // Each client sends echo requests to the next node
    UdpEchoClientHelper echoClient(Ipv4Address("192.9.39.0"), 9); // Dummy address, will be set per client
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));     // Limit each client to 20 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5))); // Half-second intervals
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1KB packets

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<Node> clientNode = nodes.Get(i);
        Ptr<Node> serverNode = nodes.Get((i + 1) % numNodes); // Next node in the ring

        // Get the IP address of the server node for this client
        Ipv4Address serverAddress = serverNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
        echoClient.SetRemoteAddress(serverAddress); // Set the destination IP for this client

        clientApps.Add(echoClient.Install(clientNode));
    }
    clientApps.Start(Seconds(2.0)); // Clients start after servers and network setup
    clientApps.Stop(Seconds(simulationTime));

    // 7. Integrate a flow monitor to collect statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // 8. Generate an animation file to visualize network activity (NetAnim)
    AnimationInterface anim("adhoc-grid-animation.xml");
    anim.SetStartTime(Seconds(0));
    anim.SetStopTime(Seconds(simulationTime));
    anim.EnablePacketMetadata();      // Show packet information in animation
    anim.EnableIpv4RouteTracking();   // Track IPv4 routing table changes

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // 9. Output flow statistics and performance metrics to the console
    monitor->CheckForDeadFlows();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetFlowClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\n*** Flow Monitor Statistics ***" << std::endl;
    std::cout << std::fixed << std::setprecision(2); // For consistent output formatting

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);

        // Get flow statistics
        double packetsSent = iter->second.txPackets;
        double packetsReceived = iter->second.rxPackets;
        double bytesReceived = iter->second.rxBytes;
        Time delaySum = iter->second.delaySum;
        Time timeFirstTxPacket = iter->second.timeFirstTxPacket;
        Time timeLastRxPacket = iter->second.timeLastRxPacket;

        // Calculate metrics
        double pdr = (packetsSent > 0) ? (static_cast<double>(packetsReceived) / packetsSent) * 100.0 : 0.0;
        double endToEndDelay = (packetsReceived > 0) ? delaySum.GetSeconds() / packetsReceived : 0.0;
        double throughput = 0.0;
        if (timeLastRxPacket.GetSeconds() > timeFirstTxPacket.GetSeconds()) {
            throughput = (bytesReceived * 8.0) / (timeLastRxPacket.GetSeconds() - timeFirstTxPacket.GetSeconds()) / (1024 * 1024); // Convert to Mbps
        }

        std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << ":" << t.sourcePort
                  << " -> " << t.destinationAddress << ":" << t.destinationPort << ")\n";
        std::cout << "  Packets Sent: " << packetsSent << "\n";
        std::cout << "  Packets Received: " << packetsReceived << "\n";
        std::cout << "  Packet Delivery Ratio (PDR): " << pdr << " %\n";
        std::cout << "  End-to-End Delay: " << endToEndDelay << " s\n";
        std::cout << "  Throughput: " << throughput << " Mbps\n";
        std::cout << "-------------------------------------------\n";
    }

    Simulator::Destroy();

    return 0;
}