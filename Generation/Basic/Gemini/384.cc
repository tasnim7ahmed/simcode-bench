#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

// Define a log component for this example
NS_LOG_COMPONENT_DEFINE("SimpleTcpPointToPoint");

int
main(int argc, char* argv[])
{
    // Set up logging
    LogComponentEnable("SimpleTcpPointToPoint", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create two nodes
    NS_LOG_INFO("Creating nodes.");
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    NodeContainer nodes(n0, n1);

    // Configure point-to-point link
    NS_LOG_INFO("Creating point-to-point link.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install internet stack on nodes
    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up PacketSink (server) on Node 1
    NS_LOG_INFO("Setting up PacketSink on Node 1.");
    uint16_t port = 9; // Common port for testing
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(n1);
    serverApps.Start(Seconds(1.0)); // Server starts at 1s
    serverApps.Stop(Seconds(5.0));  // Server stops at 5s (same as simulation end)

    // Set up BulkSendApplication (client) on Node 0
    NS_LOG_INFO("Setting up BulkSendApplication on Node 0.");
    BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(1), port));
    // Set max bytes to 0 to send until stop time
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApps = clientHelper.Install(n0);
    clientApps.Start(Seconds(2.0)); // Client starts sending at 2s
    clientApps.Stop(Seconds(5.0));  // Client stops sending at 5s

    // Run the simulation
    NS_LOG_INFO("Running simulation.");
    Simulator::Stop(Seconds(5.0)); // Simulation stops at 5s
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}