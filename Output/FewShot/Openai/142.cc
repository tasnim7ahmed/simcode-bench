#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 0 (peripheral), 1 (central), 2 (peripheral)
    NodeContainer starNodes;
    starNodes.Create(3);

    // Connect node 0 <-> 1 and node 2 <-> 1 via point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Links: (0,1) and (2,1)
    NetDeviceContainer devA = p2p.Install(starNodes.Get(0), starNodes.Get(1));
    NetDeviceContainer devB = p2p.Install(starNodes.Get(2), starNodes.Get(1));

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(starNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    // Subnet for 0 <-> 1
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceA = address.Assign(devA);

    // Subnet for 2 <-> 1
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceB = address.Assign(devB);

    // Set up static routing so node 0 and node 2 can reach each other via node 1
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    Ptr<Ipv4> ipv4_0 = starNodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting0 = ipv4RoutingHelper.GetStaticRouting(ipv4_0);
    staticRouting0->AddNetworkRouteTo(
        Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), ifaceA.GetAddress(1), 1);

    Ptr<Ipv4> ipv4_2 = starNodes.Get(2)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting2 = ipv4RoutingHelper.GetStaticRouting(ipv4_2);
    staticRouting2->AddNetworkRouteTo(
        Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), ifaceB.GetAddress(1), 1);

    // Node 2 will run a UDP Echo Server
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(starNodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Node 0 will run a UDP Echo Client targeting node 2's address on port 9
    UdpEchoClientHelper echoClient(ifaceB.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = echoClient.Install(starNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}