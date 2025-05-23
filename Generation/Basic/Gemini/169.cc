#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h" // Optional, but good for detailed stats, though problem just asks for sink output

// NS_LOG_COMPONENT_DEFINE("SimpleTcpP2P"); // Not needed if no commentary

int main(int argc, char *argv[])
{
    // Enable logging for specific components for debugging (optional, but good practice)
    // NS_LOG_INFO("Creating topology");

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2); // Two nodes: client and server

    // 2. Configure Point-to-Point link
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));

    // 3. Install devices on nodes
    NetDeviceContainer devices;
    devices = p2pHelper.Install(nodes);

    // 4. Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Server node IP: interfaces.GetAddress(1) (node 1 is the server)
    // Client node IP: interfaces.GetAddress(0) (node 0 is the client)

    // 6. Setup Applications

    // Server: PacketSink Application
    uint16_t port = 9; // Common port for sink
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port)); // Server is node 1

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1)); // Install on server node (node 1)
    serverApps.Start(Seconds(0.0));                                       // Start immediately
    serverApps.Stop(Seconds(10.0));                                       // Stop at simulation end

    // Client: BulkSend Application
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", serverAddress);
    // Set the amount of data to send (0 means unlimited)
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0)); // Install on client node (node 0)
    clientApps.Start(Seconds(1.0));                                         // Start sending after 1 second
    clientApps.Stop(Seconds(10.0));                                         // Stop at simulation end

    // 7. Enable PCAP tracing
    p2pHelper.EnablePcapAll("simple-tcp-p2p"); // Traces all point-to-point devices

    // 8. Set simulation time
    Simulator::Stop(Seconds(10.0));

    // 9. Run simulation
    Simulator::Run();

    // 10. Observe throughput at the server
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));
    double totalRxBytes = sink->GetTotalRxBytes();
    double simulationDuration = Simulator::Now().GetSeconds(); // Actual simulation duration

    // The client application runs from 1.0s to 10.0s, so for 9 seconds.
    // The server application is active for the whole duration, but actual reception
    // happens during the client's active time.
    double appDuration = clientApps.Get(0)->GetStopTime().GetSeconds() - clientApps.Get(0)->GetStartTime().GetSeconds();

    if (appDuration > 0)
    {
        double throughputMbps = (totalRxBytes * 8.0) / (appDuration * 1000000.0); // Convert bytes to bits, then to Mbps
        // NS_LOG_INFO("Simulation finished.");
        std::cout << "Total Bytes Received at Server: " << totalRxBytes << " bytes" << std::endl;
        std::cout << "Application Duration: " << appDuration << " seconds" << std::endl;
        std::cout << "Throughput at Server: " << throughputMbps << " Mbps" << std::endl;
    }
    else
    {
        // NS_LOG_WARN("Application duration is zero or negative, cannot calculate throughput.");
        std::cout << "No data transferred or invalid application duration." << std::endl;
    }

    // 11. Clean up
    Simulator::Destroy();

    return 0;
}