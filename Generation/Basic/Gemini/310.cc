#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE("TcpBulkSendPointToPoint"); // Not requested, so omit.

int main(int argc, char* argv[])
{
    // Set up logging (optional, but good for debugging)
    // LogComponentEnable("BulkSendHelper", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSinkHelper", LOG_LEVEL_INFO);

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Create PointToPointHelper and set link attributes
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install devices on nodes
    ns3::NetDeviceContainer devices = p2p.Install(nodes);

    // Install the Internet stack on the nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // -------------------------------------------------------------------------
    // TCP Sink on Node 1
    // -------------------------------------------------------------------------
    uint16_t port = 9; // Arbitrary port for the sink

    // Create a PacketSinkHelper
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));

    // Install the sink application on Node 1
    ns3::ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(ns3::Seconds(1.0)); // Sink starts at 1 second
    sinkApp.Stop(ns3::Seconds(10.0)); // Sink stops at 10 seconds

    // -------------------------------------------------------------------------
    // TCP Bulk Sender on Node 0
    // -------------------------------------------------------------------------

    // Create a BulkSendHelper
    // Sender (Node 0) connects to the sink's address (Node 1's IP) and port
    ns3::BulkSendHelper senderHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(interfaces.GetAddress(1), port));

    // Set MaxBytes = 0 for unlimited data transfer
    senderHelper.SetAttribute("MaxBytes", ns3::UintegerValue(0));

    // Install the sender application on Node 0
    ns3::ApplicationContainer senderApp = senderHelper.Install(nodes.Get(0));
    senderApp.Start(ns3::Seconds(2.0)); // Sender starts at 2 seconds
    senderApp.Stop(ns3::Seconds(10.0)); // Sender stops at 10 seconds (aligned with sink)

    // Set the simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(11.0)); // Run a bit longer than the applications

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up
    ns3::Simulator::Destroy();

    return 0;
}