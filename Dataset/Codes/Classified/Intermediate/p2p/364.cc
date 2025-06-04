#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6PingExample");

int main(int argc, char *argv[])
{
    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Point-to-Point link with IPv6
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install IPv6 stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Mask("/64"));
    Ipv6InterfaceContainer interfaces = address.Assign(devices);

    // Set up ICMP Echo Request (ping) from node 0 to node 1
    V4PingHelper ping(interfaces.GetAddress(1));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(2.0));
    pingApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
