#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWifiSimulation");

int
main(int argc, char* argv[])
{
    // 1. Set up simulation parameters
    uint32_t numNodes = 5; // Number of nodes in the ad-hoc network
    double simulationTime = 10.0; // seconds
    uint32_t udpPort = 9;
    uint32_t packetSize = 1024; // bytes
    DataRate dataRate("10Mbps"); // Target data rate for UDP traffic
    std::string outputFile = "simulation_results.txt";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the simulation", numNodes);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("dataRate", "Data rate for UDP traffic (e.g., 10Mbps)", dataRate);
    cmd.AddValue("outputFile", "Name of the file to save simulation results", outputFile);
    cmd.Parse(argc, argv);

    // 2. Configure logging (optional, but good for debugging)
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    NS_LOG_INFO("Creating " << numNodes << " nodes for ad-hoc WiFi simulation.");

    // 3. Create Nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 4. Configure Mobility (Random Walk 2D)
    MobilityHelper mobility;
    // Set a bounding box for node movement: 200m x 200m
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5]")); // Speed of 5 m/s
    mobility.Install(nodes);

    // 5. Configure WiFi (Ad-hoc mode)
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc (IBSS) mode

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    // Set up propagation model (e.g., Friis)
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n); // Use 802.11n standard

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 6. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 7. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 8. Set up UDP Applications (Node 1 sends to Node 0)
    // Create a UDP server on Node 0
    Ptr<Node> sinkNode = nodes.Get(0);
    Ipv4Address sinkAddress = interfaces.GetAddress(0); // IP address of Node 0

    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps = server.Install(sinkNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 0.5)); // Stop slightly after simulation time

    // Create a UDP client on Node 1, sending to Node 0
    Ptr<Node> clientNode = nodes.Get(1);
    UdpClientHelper client(sinkAddress, udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send a very large number of packets
    client.Attribute<DataRateValue>("Interval").SetDataRate(dataRate); // Control rate using Interval
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    
    ApplicationContainer clientApps = client.Install(clientNode);
    clientApps.Start(Seconds(1.0)); // Start sending after 1 second (to allow network to stabilize)
    clientApps.Stop(Seconds(simulationTime));

    // 9. Install FlowMonitor to track statistics
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // 10. Run the simulation
    NS_LOG_INFO("Running simulation for " << simulationTime << " seconds...");
    Simulator::Stop(Seconds(simulationTime + 1.0)); // Stop a bit after apps stop to collect data
    Simulator::Run();

    // 11. Collect and Output Statistics
    std::ofstream outFile(outputFile.c_str());
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Failed to open output file: " << outputFile);
        return 1;
    }

    outFile << "--- Simulation Results ---" << std::endl;
    outFile << "Simulation Time: " << simulationTime << " s" << std::endl;
    outFile << "Number of Nodes: " << numNodes << std::endl;
    outFile << "UDP Client (Node " << clientNode->GetId() << ", IP: " << interfaces.GetAddress(1) << ")" << std::endl;
    outFile << "UDP Server (Node " << sinkNode->GetId() << ", IP: " << interfaces.GetAddress(0) << ")" << std::endl;
    outFile << "Target Data Rate: " << dataRate << std::endl;
    outFile << "Packet Size: " << packetSize << " bytes" << std::endl;
    outFile << std::endl;

    monitor->CheckFor// Find the specific UDP flow from clientNode (Node 1) to sinkNode (Node 0)
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->Get">FlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // Check if this is our desired UDP flow (source, destination, protocol)
        if (t.sourceAddress == interfaces.GetAddress(1) &&
            t.destinationAddress == interfaces.GetAddress(0) &&
            t.protocol == 17) // 17 is UDP protocol number
        {
            outFile << "UDP Flow (Source: " << t.sourceAddress << ", Destination: " << t.destinationAddress << ")" << std::endl;
            outFile << "  Tx Packets: " << i->second.txPackets << std::endl;
            outFile << "  Rx Packets: " << i->second.rxPackets << std::endl;
            outFile << "  Lost Packets: " << i->second.lostPackets << std::endl;
            outFile << "  Tx Bytes: " << i->second.txBytes << std::endl;
            outFile << "  Rx Bytes: " << i->second.rxBytes << std::endl;

            double throughputMbps = (i->second.rxBytes * 8.0) / (simulationTime * 1000000.0); // Convert Bytes to Mbps
            outFile << "  Throughput: " << throughputMbps << " Mbps" << std::endl;

            double packetLossRate = 0.0;
            if (i->second.txPackets > 0)
            {
                packetLossRate = (double)i->second.lostPackets / i->second.txPackets * 100.0;
            }
            outFile << "  Packet Loss Rate: " << packetLossRate << " %" << std::endl;
            break; // Found our flow, no need to check others
        }
    }

    outFile.close();
    NS_LOG_INFO("Results saved to " << outputFile);

    // 12. Cleanup
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}