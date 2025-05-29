#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6Ping6");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_ALL);
    LogComponent::SetDefaultLogComponentEnable(true);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Enable IPv6 forwarding, for now, do it globally
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    ipv6RoutingHelper.PopulateRoutingTables();

    // Create the ping application
    V4Ping6Helper ping6;
    ping6.SetRemote(interfaces.GetAddress(1, 1));
    ping6.SetAttribute("Verbose", BooleanValue(true));

    ApplicationContainer pings = ping6.Install(nodes.Get(0));
    pings.Start(Seconds(1.0));
    pings.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}