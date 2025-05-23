#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int
main(int argc, char *argv[])
{
    // Set default time resolution to nanoseconds
    Time::SetResolution(Time::NS);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure the point-to-point link properties
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the point-to-point devices on the nodes
    NetDeviceContainer devices = p2pHelper.Install(nodes);

    // Install the Internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the PacketSink application on the second node (receiver)
    uint16_t port = 9; // Port for TCP communication
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::Any(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1)); // Install on the second node
    sinkApp.Start(Seconds(0.0)); // Start the sink at the beginning of the simulation
    sinkApp.Stop(Seconds(10.0)); // Stop the sink at the end of the simulation

    // Set up the BulkSend application on the first node (sender)
    // It will send to the IP address of the second node's interface
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    // Set MaxBytes to 0 for unlimited data
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0)); // Install on the first node
    sourceApp.Start(Seconds(1.0)); // Start sending 1 second into the simulation
    sourceApp.Stop(Seconds(10.0)); // Stop sending at the end of the simulation

    // Set the global simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Clean up resources
    Simulator::Destroy();

    return 0;
}