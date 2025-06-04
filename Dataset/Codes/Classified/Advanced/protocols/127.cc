#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinkStateRoutingExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Command-line options
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Step 1: Create 4 routers
    NodeContainer routers;
    routers.Create(4);

    // Step 2: Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create the links between the routers
    NetDeviceContainer link1 = p2p.Install(routers.Get(0), routers.Get(1)); // Link between router 0 and 1
    NetDeviceContainer link2 = p2p.Install(routers.Get(1), routers.Get(2)); // Link between router 1 and 2
    NetDeviceContainer link3 = p2p.Install(routers.Get(2), routers.Get(3)); // Link between router 2 and 3
    NetDeviceContainer link4 = p2p.Install(routers.Get(0), routers.Get(3)); // Link between router 0 and 3

    // Step 3: Install internet stacks with OSPF (Link State Routing) on the routers
    OspfHelper ospfRouting;
    InternetStackHelper internet;
    internet.SetRoutingHelper(ospfRouting);
    internet.Install(routers);

    // Step 4: Assign IP addresses to the links
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = ipv4.Assign(link1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = ipv4.Assign(link2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3 = ipv4.Assign(link3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i4 = ipv4.Assign(link4);

    // Step 5: Enable OSPF (Link State Routing) on all routers
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Step 6: Create a UDP echo server on router 3
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(3)); // Server on router 3
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Step 7: Create a UDP echo client on router 0
    UdpEchoClientHelper echoClient(i3.GetAddress(1), 9); // Send to router 3 IP
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(routers.Get(0)); // Client on router 0
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Step 8: Enable tracing (pcap files)
    p2p.EnablePcapAll("ospf-linkstate");

    // Step 9: Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

