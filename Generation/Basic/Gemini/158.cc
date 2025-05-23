#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

int main(int argc, char *argv[])
{
    // Set default values for simulation parameters
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1024;   // bytes
    std::string dataRate = "5Mbps"; // OnOffApplication data rate
    uint16_t port = 8080;         // TCP port

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Create a Point-to-Point link helper
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps")); // Link capacity must be higher than application rate
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install devices on nodes
    ns3::NetDeviceContainer devices = p2pHelper.Install(nodes);

    // Install the Internet stack on nodes
    ns3::InternetStackHelper stackHelper;
    stackHelper.Install(nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper addressHelper;
    addressHelper.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = addressHelper.Assign(devices);

    // --- TCP Server Setup ---
    // Get the server's IP address (Node 0, interface 0)
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(0);

    // Create PacketSinkHelper for the TCP server
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(serverAddress, port));

    // Install server application on Node 0
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime));

    // --- TCP Client Setup ---
    // Create OnOffHelper for the TCP client
    ns3::OnOffHelper clientHelper("ns3::TcpSocketFactory", ns3::Address(ns3::InetSocketAddress(serverAddress, port)));
    clientHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always on during active period
    clientHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Always on during active period
    clientHelper.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate(dataRate)));
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));

    // Install client application on Node 1
    ns3::ApplicationContainer clientApps = clientHelper.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10 seconds

    // --- FlowMonitor Setup ---
    ns3::FlowMonitorHelper flowmonHelper;
    // Install FlowMonitor on all nodes
    ns3::Ptr<ns3::FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));

    // Run the simulation
    ns3::Simulator::Run();

    // --- FlowMonitor Statistics Collection and Printing ---
    // Check for both TX and RX
    flowMonitor->CheckFor = ns3::FlowMonitor::CHECK_TX_RX;

    // Get statistics
    ns3::Ptr<ns3::Ipv4FlowClassifier> classifier = ns3::DynamicCast<ns3::Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    ns3::FlowMonitor::StatsMap stats = flowMonitor->GetStats();

    std::cout << "\n--- FlowMonitor Statistics ---\n";
    for (const auto& flowEntry : stats)
    {
        ns3::Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowEntry.first);
        
        // Calculate the duration for throughput based on actual packet reception times
        // If no packets received, or start/end times are problematic, fall back to simulation time
        double flowDuration = 0.0;
        if (flowEntry.second.timeLastRxPacket.IsPositive() && flowEntry.second.timeFirstTxPacket.IsPositive()) {
            flowDuration = flowEntry.second.timeLastRxPacket.GetSeconds() - flowEntry.second.timeFirstTxPacket.GetSeconds();
        }
        
        // Ensure flowDuration is positive for division
        if (flowDuration <= 0) {
            flowDuration = simulationTime; // Fallback to total simulation time if flow duration cannot be determined or is zero
        }

        std::cout << "Flow ID: " << flowEntry.first << " (" << t.protocol << ")\n";
        std::cout << "  Source IP: " << t.sourceAddress << " Port: " << t.sourcePort << "\n";
        std::cout << "  Destination IP: " << t.destinationAddress << " Port: " << t.destinationPort << "\n";
        std::cout << "  Packets Sent: " << flowEntry.second.txPackets << "\n";
        std::cout << "  Bytes Sent: " << flowEntry.second.txBytes << "\n";
        std::cout << "  Packets Received: " << flowEntry.second.rxPackets << "\n";
        std::cout << "  Bytes Received: " << flowEntry.second.rxBytes << "\n";
        
        // Calculate throughput (received bytes converted to Mbps)
        double throughputMbps = (flowEntry.second.rxBytes * 8.0) / (flowDuration * 1e6);
        std::cout << "  Average Throughput (active): " << throughputMbps << " Mbps\n";
        
        // Goodput over the entire simulation time
        double goodputMbps = (flowEntry.second.rxBytes * 8.0) / (simulationTime * 1e6);
        std::cout << "  Goodput (over simulation): " << goodputMbps << " Mbps\n";
        
        std::cout << "--------------------------------\n";
    }

    // Clean up
    ns3::Simulator::Destroy();
    flowMonitor->Destroy(); // Important to destroy the FlowMonitor object explicitly

    return 0;
}