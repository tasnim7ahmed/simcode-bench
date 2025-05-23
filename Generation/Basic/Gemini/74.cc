#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-variable-stream.h"
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip> // For std::fixed and std::setprecision

// Namespace for convenience
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

// Global parameters with default values
static uint32_t numNodes = 50;
static double simTime = 200.0; // seconds
static double nodeSpeed = 20.0; // m/s
static double txPowerDbM = 7.5; // dBm
static bool tracingMobility = false;
static bool flowMonitor = false;
static std::string protocol = "AODV"; // Default routing protocol
static std::string outputFile = "manet-simulation-results.csv"; // FlowMonitor output file

int main(int argc, char *argv[])
{
    // 1. Set up command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes", numNodes);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.AddValue("nodeSpeed", "Node speed in m/s for RandomWaypointMobilityModel", nodeSpeed);
    cmd.AddValue("txPowerDbM", "WiFi transmission power in dBm", txPowerDbM);
    cmd.AddValue("protocol", "Routing protocol (AODV, DSDV, OLSR, DSR)", protocol);
    cmd.AddValue("tracingMobility", "Enable mobility tracing (ASCII file 'mobility.mob')", tracingMobility);
    cmd.AddValue("flowMonitor", "Enable FlowMonitor (outputs CSV to 'outputFile')", flowMonitor);
    cmd.AddValue("outputFile", "CSV file to output FlowMonitor statistics", outputFile);
    cmd.Parse(argc, argv);

    // 2. Set up logging and random seed
    LogComponentEnable("ManetSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    if (flowMonitor) {
        LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);
    }
    // Set a fixed seed for reproducibility across runs
    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(1);

    // 3. Create nodes
    NS_LOG_INFO("Creating " << numNodes << " nodes.");
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 4. Install mobility model (RandomWaypoint)
    NS_LOG_INFO("Installing RandomWaypointMobilityModel.");
    MobilityHelper mobility;
    // Define speed range (single value for fixed speed)
    Ptr<UniformRandomVariable> speedRv = CreateObject<UniformRandomVariable>();
    speedRv->SetAttribute("Min", DoubleValue(nodeSpeed));
    speedRv->SetAttribute("Max", DoubleValue(nodeSpeed));
    // Define pause time (0 for continuous movement)
    Ptr<UniformRandomVariable> pauseRv = CreateObject<UniformRandomVariable>();
    pauseRv->SetAttribute("Min", DoubleValue(0.0));
    pauseRv->SetAttribute("Max", DoubleValue(0.0));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", PointerValue(speedRv),
                              "Pause", PointerValue(pauseRv),
                              "PositionAllocator", CreateObject<RandomRectanglePositionAllocator>(
                                  Rectangle(0, 300, 0, 1500) // 300x1500 m area
                              ));
    mobility.Install(nodes);

    // 5. Install WiFi devices and configure PHY/MAC
    NS_LOG_INFO("Installing WiFi devices with ad-hoc mode and " << txPowerDbM << " dBm Tx Power.");
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    // Set transmission power
    wifiPhy.Set("TxPowerStart", DoubleValue(txPowerDbM));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerDbM));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 6. Install Internet stack with selected routing protocol
    NS_LOG_INFO("Installing Internet stack with " << protocol << " routing protocol.");
    InternetStackHelper internetStack;
    if (protocol == "AODV") {
        AodvHelper aodv;
        internetStack.SetRoutingHelper(aodv);
    } else if (protocol == "DSDV") {
        DsdvHelper dsdv;
        internetStack.SetRoutingHelper(dsdv);
    } else if (protocol == "OLSR") {
        OlsrHelper olsr;
        internetStack.SetRoutingHelper(olsr);
    } else if (protocol == "DSR") {
        DsrHelper dsr;
        internetStack.SetRoutingHelper(dsr);
    } else {
        NS_FATAL_ERROR("Unknown protocol: " << protocol << ". Choose from AODV, DSDV, OLSR, DSR.");
    }
    internetStack.Install(nodes);

    // 7. Assign IP addresses
    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 8. Create UDP applications (OnOffClient/PacketSink)
    NS_LOG_INFO("Creating 10 UDP source/sink pairs.");
    uint16_t port = 9; // Starting port number for applications
    Ptr<UniformRandomVariable> startTimeRv = CreateObject<UniformRandomVariable>();
    startTimeRv->SetAttribute("Min", DoubleValue(50.0));
    startTimeRv->SetAttribute("Max", DoubleValue(51.0));

    ApplicationContainer clientApps, serverApps;

    for (uint32_t i = 0; i < 10; ++i) {
        // Randomly select source and sink nodes, ensuring source != sink
        uint32_t srcNodeId = Simulator::GetRng()->GetValue(0, numNodes - 1);
        uint32_t sinkNodeId = Simulator::GetRng()->GetValue(0, numNodes - 1);
        while (srcNodeId == sinkNodeId) {
            sinkNodeId = Simulator::GetRng()->GetValue(0, numNodes - 1);
        }

        Ptr<Node> sender = nodes.Get(srcNodeId);
        Ptr<Node> receiver = nodes.Get(sinkNodeId);
        Ipv4Address sinkAddress = interfaces.GetAddress(sinkNodeId);

        // PacketSink (server) on receiver
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        serverApps.Add(sinkHelper.Install(receiver));

        // OnOffApplication (client) on sender
        OnOffHelper clientHelper("ns3::UdpSocketFactory", InetSocketAddress(sinkAddress, port));
        // Set OnTime to constant 1.0s and OffTime to constant 0.0s for continuous traffic
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        clientHelper.SetAttribute("DataRate", StringValue("1Mbps")); // 1 Mbps data rate
        clientHelper.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes packet size

        Ptr<Application> clientApp = clientHelper.Install(sender).Get(0);
        clientApp->SetStartTime(Seconds(startTimeRv->GetValue()));
        clientApp->SetStopTime(Seconds(simTime)); // Applications run until end of simulation

        clientApps.Add(clientApp);
        port++; // Use different port for each flow to distinguish them easily in FlowMonitor
    }
    serverApps.SetStartTime(Seconds(0.0));
    serverApps.SetStopTime(Seconds(simTime));

    // 9. Enable tracing and flow monitoring (optional)
    if (tracingMobility) {
        NS_LOG_INFO("Enabling mobility tracing to mobility.mob.");
        AsciiTraceHelper ascii;
        mobility.EnableAsciiAll(ascii.CreateFileStream("mobility.mob"));
    }

    Ptr<FlowMonitor> flowMonitorPtr;
    FlowMonitorHelper flowMonitorHelper;
    if (flowMonitor) {
        NS_LOG_INFO("Enabling FlowMonitor.");
        flowMonitorPtr = flowMonitorHelper.InstallAll();
    }

    // 10. Run simulation
    NS_LOG_INFO("Running simulation for " << simTime << " seconds.");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // 11. Collect and output FlowMonitor statistics (if enabled)
    if (flowMonitor) {
        NS_LOG_INFO("Generating FlowMonitor statistics to " << outputFile << ".");
        flowMonitorPtr->CheckForLostPackets(); // Ensure lost packets are counted
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
        FlowMonitor::FlowStatsContainer stats = flowMonitorPtr->GetFlowStats();

        std::ofstream outFile(outputFile.c_str(), std::ios_base::out | std::ios_base::trunc);
        outFile << "FlowId,SourceIp,DestIp,SourcePort,DestPort,Protocol,PacketsSent,BytesSent,PacketsReceived,BytesReceived,PacketLoss,ThroughputKbps\n";

        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            outFile << i->first << ",";
            outFile << t.sourceAddress << "," << t.destinationAddress << ",";
            outFile << t.sourcePort << "," << t.destinationPort << "," << (uint32_t)t.protocol << ",";
            outFile << i->second.txPackets << "," << i->second.txBytes << ",";
            outFile << i->second.rxPackets << "," << i->second.rxBytes << ",";
            outFile << i->second.lostPackets << ",";
            
            // Calculate throughput: (Received Bytes * 8 bits/byte) / (Time * 1000 bits/Kb)
            // Use the effective flow duration from the first observed packet to the end of simulation
            // or the last received packet time, whichever is greater.
            double flowDuration = simTime - t.start.GetSeconds();
            if (flowDuration <= 0.0) { // Should not happen for active flows, but for safety
                flowDuration = 1.0; // Avoid division by zero
            }

            double throughput = (i->second.rxBytes * 8.0 / flowDuration / 1000.0); // Kbps

            outFile << std::fixed << std::setprecision(3) << throughput << "\n";
        }
        outFile.close();
        NS_LOG_INFO("FlowMonitor statistics saved to " << outputFile << ".");
    } else {
        NS_LOG_INFO("FlowMonitor not enabled. No detailed reception statistics generated.");
    }

    // 12. Cleanup
    NS_LOG_INFO("Simulation finished. Cleaning up.");
    Simulator::Destroy();

    return 0;
}