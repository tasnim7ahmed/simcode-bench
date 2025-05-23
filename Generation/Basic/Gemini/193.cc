#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimplePointToPointTcp");

int main(int argc, char *argv[])
{
    // Enable logging for this example
    LogComponentEnable("SimplePointToPointTcp", LOG_LEVEL_INFO);
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSinkApplication", LOG_LEVEL_INFO);

    // Default simulation parameters
    double simulationTime = 10.0; // seconds
    uint16_t port = 9;           // Port for the PacketSink application

    // Command line argument parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("port", "Port number for the PacketSink application", port);
    cmd.Parse(argc, argv);

    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create(2); // Node 0 is sender, Node 1 is receiver

    // 2. Configure Point-to-Point channel characteristics
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 3. Install the Internet stack on both nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 5. Setup Applications (TCP)
    // Configure the receiver (Node 1) with a PacketSink application
    // The PacketSink listens on an IP address and port using a TCP socket factory.
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1)); // Install on receiver (Node 1)
    sinkApps.Start(Seconds(0.0));                                            // Start sink at the beginning of simulation
    sinkApps.Stop(Seconds(simulationTime + 1.0));                            // Ensure sink runs longer than sender

    // Get a pointer to the PacketSink application instance to retrieve statistics later
    Ptr<PacketSink> sink = StaticCast<PacketSink>(sinkApps.Get(0));

    // Configure the sender (Node 0) with a BulkSend application
    // The BulkSend application sends data to the sinkAddress using a TCP socket factory.
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    // Set MaxBytes to 0 to continuously send data until the application stops
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0)); // Install on sender (Node 0)
    clientApps.Start(Seconds(1.0));                                          // Start sending after 1 second (to allow connection setup)
    clientApps.Stop(Seconds(simulationTime));                                // Stop sending at simulationTime

    // 6. Enable packet tracing to generate pcap files
    // This will generate pcap files for all devices installed by pointToPointHelper.
    // For example, "simple-p2p-tcp-0-0.pcap" and "simple-p2p-tcp-0-1.pcap".
    pointToPoint.EnablePcapAll("simple-p2p-tcp");

    // Set the simulation stop time
    Simulator::Stop(Seconds(simulationTime + 1.0));

    // Run the simulation
    Simulator::Run();

    // 7. Log the total amount of data received at the end of the simulation
    std::cout << "Total data received at receiver (Node 1): " << sink->GetTotalRx() << " bytes" << std::endl;

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}