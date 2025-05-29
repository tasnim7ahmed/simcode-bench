#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    uint32_t packetSize = 56;
    uint32_t maxPackets = 5;
    Time interPacketInterval = Seconds(1.0);

    V6PingHelper pingHelper(interfaces.GetAddress(1, 1));
    pingHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    pingHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    pingHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
    ApplicationContainer apps = pingHelper.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}