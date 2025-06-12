#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/icmpv6-echo.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for ICMPv6 Echo applications
    LogComponentEnable("Icmpv6EchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6EchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point channel with given attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install net devices
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack with IPv6
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    // Start the server on node 1
    uint16_t echoPort = 9; // not used, kept for convention
    Icmpv6EchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Create the client on node 0
    Ipv6Address serverAddress = interfaces.GetAddress(1, 1); // global address, index 1
    Icmpv6EchoClientHelper echoClient(serverAddress);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}