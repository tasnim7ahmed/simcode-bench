#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PNetworkSimulation");

int main(int argc, char *argv[])
{
    // Enable logging for UdpEchoClient and UdpEchoServer applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup Point-to-Point link with 5 Mbps data rate and 2 ms delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Assign IPv4 addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP echo server on node 1 (second node)
    UdpEchoServerHelper echoServer(9); // Port number 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP echo client on node 0 (first node)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9); // Server IP and port
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));     // Send 5 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval between packets
    echoClient.SetAttribute("PacketSize", UintegerValue(512));   // Packet size in bytes

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}