#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    interfaces.SetForwarding(1, true);
    interfaces.SetDefaultRouteInAllNodes(1);

    uint32_t packetSize = 56;
    uint32_t maxPackets = 5;
    Time interPacketInterval = Seconds(1.0);

    Ipv6Address remoteAddr = interfaces.GetAddress(1, 1);

    V6PingHelper ping(remoteAddr);
    ping.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ping.SetAttribute("Count", UintegerValue(maxPackets));

    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}