#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

// Using namespace ns3 is common in ns-3 examples
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

// Global variables for simulation parameters, configurable via command line
double g_simTime = 100.0; // seconds
uint32_t g_numNodes = 4;
double g_xMax = 200.0; // meters (mobility boundary)
double g_yMax = 200.0; // meters (mobility boundary)
uint32_t g_packetSize = 1024; // bytes (UDP payload size)
std::string g_dataRate = "1Mbps"; // UDP data rate
double g_appStartTime = 1.0; // seconds (when UDP traffic starts)
double g_appStopTime = g_simTime - 1.0; // seconds (when UDP traffic stops)

int main(int argc, char *argv[])
{
    // Enable logging for specific components
    LogComponentEnable("AodvManetSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL); // Uncomment for detailed AODV logs
    // LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL);      // Uncomment for detailed IP layer logs

    // Allow parameters to be set from the command line
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", g_simTime);
    cmd.AddValue("numNodes", "Number of nodes in the MANET", g_numNodes);
    cmd.AddValue("xMax", "X-coordinate boundary for node mobility", g_xMax);
    cmd.AddValue("yMax", "Y-coordinate boundary for node mobility", g_yMax);
    cmd.AddValue("packetSize", "UDP packet size in bytes", g_packetSize);
    cmd.AddValue("dataRate", "UDP data rate (e.g., 1Mbps, 512Kbps)", g_dataRate);
    cmd.Parse(argc, argv);

    // Validate the number of nodes
    if (g_numNodes < 2)
    {
        NS_FATAL_ERROR("Number of nodes must be at least 2 for communication.");
    }

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(g_numNodes);

    // 2. Configure mobility for nodes
    // Using RandomWalk2dMobilityModel for random movement within a defined boundary
    MobilityHelper mobility;
    
    // Set initial positions in a grid-like fashion for better visibility initially
    mobility.SetPositionAllocator(
        "ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(50.0), // Spacing between nodes
        "DeltaY", DoubleValue(50.0),
        "GridWidth", UintegerValue(2), // Max 2 nodes per row
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0, g_xMax, 0, g_yMax)), // Movement boundary
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10.0]"), // Constant speed of 10 m/s
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")   // Pause for 1 second after each move
    );
    mobility.Install(nodes);

    // 3. Setup WiFi devices for Ad-Hoc network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ); // Use a modern 802.11 standard

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Create());
    // Set transmit power to a reasonable value for mobile nodes
    wifiPhy.Set("TxPowerStart", DoubleValue(15.0)); // dBm
    wifiPhy.Set("TxPowerEnd", DoubleValue(15.0));   // dBm

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode is essential for MANETs

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 4. Install AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internet.Install(nodes);

    // 5. Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0"); // Define the network base address and subnet mask
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Setup UDP applications: Node 0 (source) sends to Node 1 (sink)
    // Get IP addresses for source and sink nodes
    Ptr<Ipv4> ipv4Src = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4Dst = nodes.Get(1)->GetObject<Ipv4>();
    // GetLocal() with index 1 refers to the IP address of the first real interface (index 0 is localhost)
    Ipv4Address sinkAddress = ipv4Dst->GetAddress(1, 0).GetLocal(); 
    Ipv4Address sourceAddress = ipv4Src->GetAddress(1, 0).GetLocal();

    uint16_t sinkPort = 9; // Common port for echo/sink applications

    // Packet Sink application (receives UDP packets) on Node 1
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(g_appStartTime));
    sinkApps.Stop(Seconds(g_appStopTime));

    // OnOff application (sends UDP packets) from Node 0
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(sinkAddress, sinkPort));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // Always On
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // No Off period
    onoff.SetAttribute("PacketSize", UintegerValue(g_packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(g_dataRate)));
    ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(Seconds(g_appStartTime));
    clientApps.Stop(Seconds(g_appStopTime));

    // 7. Enable PCAP tracing for all wireless devices
    // This will generate .pcap files in the current directory for analysis with Wireshark
    wifiPhy.EnablePcapAll("aodv-manet-trace");

    // 8. Setup Flow Monitor for detailed performance analysis
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowMonitorHelper;
    flowMonitor = flowMonitorHelper.InstallAll();

    // 9. Enable NetAnim visualization
    // This will generate an XML file for visualization with NetAnim (ns-3 tool)
    AnimationInterface anim("aodv-manet-anim.xml");
    anim.SetStartTime(Seconds(0));
    anim.SetStopTime(Seconds(g_simTime));
    for (uint32_t i = 0; i < g_numNodes; ++i) {
        anim.SetNodeDescription(nodes.Get(i), "Node " + std::to_string(i));
        anim.SetNodeColor(nodes.Get(i), 0, 255, 0); // Default color: Green
    }
    // Highlight source and sink nodes with different colors
    anim.SetNodeColor(nodes.Get(0), 255, 0, 0); // Red for source (Node 0)
    anim.SetNodeColor(nodes.Get(1), 0, 0, 255); // Blue for sink (Node 1)

    // 10. Run simulation
    Simulator::Stop(Seconds(g_simTime)); // Ensure simulation stops at g_simTime
    Simulator::Run();

    // 11. Process Flow Monitor results to calculate throughput and packet loss
    flowMonitor->CheckFor = (flowMonitorHelper.GetFlowStats()); // Collect statistics before destruction
    double totalThroughputMbps = 0.0;
    uint32_t totalPacketsSent = 0;
    uint32_t totalPacketsReceived = 0;
    uint32_t totalPacketsLost = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowMonitor->GetFlowStats().begin();
         i != flowMonitor->GetFlowStats().end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->Classify(i->first);

        // Filter results for the specific flow from source (Node 0) to sink (Node 1)
        if (t.sourceAddress == sourceAddress && t.destinationAddress == sinkAddress)
        {
            totalPacketsSent += i->second.txPackets;
            totalPacketsReceived += i->second.rxPackets;
            totalPacketsLost += i->second.lostPackets;

            // Calculate throughput for this flow
            if (i->second.timeLastRxPacket.GetSeconds() > i->second.timeFirstTxPacket.GetSeconds())
            {
                double flowDuration = i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
                double throughput = (i->second.rxBytes * 8.0) / flowDuration / 1024 / 1024; // Convert bytes to Mbits/s
                totalThroughputMbps += throughput;
            }
        }
    }

    // Print aggregated simulation results
    std::cout << "\n--- Simulation Results ---" << std::endl;
    std::cout << "  Nodes: " << g_numNodes << std::endl;
    std::cout << "  Simulation Time: " << g_simTime << " s" << std::endl;
    std::cout << "  Source (Node 0) IP: " << sourceAddress << std::endl;
    std::cout << "  Sink (Node 1) IP: " << sinkAddress << std::endl;
    std::cout << "  Total Packets Sent: " << totalPacketsSent << std::endl;
    std::cout << "  Total Packets Received: " << totalPacketsReceived << std::endl;
    std::cout << "  Total Packets Lost: " << totalPacketsLost << std::endl;

    double packetLossRatio = 0.0;
    if (totalPacketsSent > 0)
    {
        packetLossRatio = (double)totalPacketsLost / totalPacketsSent;
    }
    std::cout << "  Packet Loss Ratio: " << packetLossRatio << std::endl;
    std::cout << "  Throughput (Node 0 to Node 1): " << totalThroughputMbps << " Mbps" << std::endl;

    // 12. Cleanup simulation resources
    Simulator::Destroy();

    return 0;
}