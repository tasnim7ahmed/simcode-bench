#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bulk-send-helper.h"

int main()
{
    // Enable logging for BulkSendHelper and PacketSink for visibility
    LogComponentEnable("BulkSendHelper", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2); // Node 0 and Node 1

    // Configure Point-to-Point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack on both nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses from the 10.1.1.0/24 subnet
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure TCP Server (PacketSink) on Node 1
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port)); // Listen on any address
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);

    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1)); // Node 1 is the server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Configure TCP Client (BulkSendHelper) on Node 0
    // Client sends to Node 1's IP address (interfaces.GetAddress(1)) on the specified port
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                  InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Set to 0 for unlimited data
    
    ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0)); // Node 0 is the client
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set the global simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}