#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/icmpv6-echo.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = address.Assign(devices);

    interfaces.SetForwarding(0, true);
    interfaces.SetForwarding(1, true);

    // Enable IPv6 autoconfiguration
    for (uint32_t i = 0; i < interfaces.GetNInterfaces(); ++i) {
        interfaces.GetAddress(i, 1);
    }

    Icmpv6EchoClientHelper ping;
    ping.SetRemote(interfaces.GetAddress(1, 1), Ipv6Address::GetZero());
    ping.SetAttribute("Verbose", BooleanValue(true));

    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}