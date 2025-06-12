#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyUdpEcho");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point helper for links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes in a ring: 0-1, 1-2, 2-3, 3-0
    NetDeviceContainer devices[4];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    address.SetBase("192.9.39.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);

    // Set up UDP Echo Server and Client applications
    uint16_t port = 9;

    // One server-client pair at a time
    // First run: Node 0 as client, Node 1 as server
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Second run: Node 1 as client, Node 2 as server (after first finishes)
    UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(6.0));
    serverApps2.Stop(Seconds(15.0));

    UdpEchoClientHelper echoClient2(interfaces[1].GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(7.0));
    clientApps2.Stop(Seconds(15.0));

    // Third run: Node 2 as client, Node 3 as server
    UdpEchoServerHelper echoServer3(port);
    ApplicationContainer serverApps3 = echoServer3.Install(nodes.Get(3));
    serverApps3.Start(Seconds(11.0));
    serverApps3.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient3(interfaces[2].GetAddress(1), port);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(2));
    clientApps3.Start(Seconds(12.0));
    clientApps3.Stop(Seconds(20.0));

    // Fourth run: Node 3 as client, Node 0 as server
    UdpEchoServerHelper echoServer4(port);
    ApplicationContainer serverApps4 = echoServer4.Install(nodes.Get(0));
    serverApps4.Start(Seconds(16.0));
    serverApps4.Stop(Seconds(25.0));

    UdpEchoClientHelper echoClient4(interfaces[3].GetAddress(1), port);
    echoClient4.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient4.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps4 = echoClient4.Install(nodes.Get(3));
    clientApps4.Start(Seconds(17.0));
    clientApps4.Stop(Seconds(25.0));

    // Setup global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}