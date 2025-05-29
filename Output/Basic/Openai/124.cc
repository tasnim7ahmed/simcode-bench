#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4); // n0, n1, n2, n3(hub)

    // For each "VLAN"/subnet, connect the node and hub in a separate CSMA segment
    NodeContainer net1Nodes;
    net1Nodes.Add(nodes.Get(0)); // n0
    net1Nodes.Add(nodes.Get(3)); // hub

    NodeContainer net2Nodes;
    net2Nodes.Add(nodes.Get(1)); // n1
    net2Nodes.Add(nodes.Get(3)); // hub

    NodeContainer net3Nodes;
    net3Nodes.Add(nodes.Get(2)); // n2
    net3Nodes.Add(nodes.Get(3)); // hub

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer net1Dev = csma.Install(net1Nodes);
    NetDeviceContainer net2Dev = csma.Install(net2Nodes);
    NetDeviceContainer net3Dev = csma.Install(net3Nodes);

    // IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper addr1, addr2, addr3;
    addr1.SetBase("10.1.1.0", "255.255.255.0");
    addr2.SetBase("10.1.2.0", "255.255.255.0");
    addr3.SetBase("10.1.3.0", "255.255.255.0");

    Ipv4InterfaceContainer i1 = addr1.Assign(net1Dev);
    Ipv4InterfaceContainer i2 = addr2.Assign(net2Dev);
    Ipv4InterfaceContainer i3 = addr3.Assign(net3Dev);

    // Set up routing: Hub as router (n3)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    Ptr<Ipv4> ipv4Hub = nodes.Get(3)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> hubStaticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4Hub);

    // Set default routes on each leaf node to their interface on the hub
    for (uint32_t i = 0; i < 3; ++i)
    {
        Ptr<Node> n = nodes.Get(i);
        Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);
        Ipv4Address gw;
        if (i == 0) 
            gw = i1.GetAddress(1);
        else if (i == 1)
            gw = i2.GetAddress(1);
        else
            gw = i3.GetAddress(1);

        staticRouting->SetDefaultRoute(gw, 1);
    }

    // Routing interface indexes: on the hub, assign for each subnet
    hubStaticRouting->AddNetworkRouteTo("10.1.1.0", "255.255.255.0", 1);
    hubStaticRouting->AddNetworkRouteTo("10.1.2.0", "255.255.255.0", 2);
    hubStaticRouting->AddNetworkRouteTo("10.1.3.0", "255.255.255.0", 3);

    // Set up simple applications to demonstrate connectivity
    uint16_t port = 7;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps;
    serverApps.Add(echoServer.Install(nodes.Get(0))); // On n0
    serverApps.Add(echoServer.Install(nodes.Get(1))); // On n1
    serverApps.Add(echoServer.Install(nodes.Get(2))); // On n2
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(i2.GetAddress(0), port); // n0->n1
    echoClient1.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(64));

    UdpEchoClientHelper echoClient2(i3.GetAddress(0), port); // n1->n2
    echoClient2.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(64));

    UdpEchoClientHelper echoClient3(i1.GetAddress(0), port); // n2->n0
    echoClient3.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient1.Install(nodes.Get(0)));
    clientApps.Add(echoClient2.Install(nodes.Get(1)));
    clientApps.Add(echoClient3.Install(nodes.Get(2)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    csma.EnablePcapAll("hub-vlan-trunk", true);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}