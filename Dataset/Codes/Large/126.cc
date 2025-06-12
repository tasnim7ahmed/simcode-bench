#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rip-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DVRSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Command-line options
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Step 1: Create nodes (3 routers)
    NodeContainer routers;
    routers.Create(3);

    // Step 2: Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerLink1 = p2p.Install(routers.Get(0), routers.Get(1)); // Link between router 0 and router 1
    NetDeviceContainer routerLink2 = p2p.Install(routers.Get(1), routers.Get(2)); // Link between router 1 and router 2
    NetDeviceContainer routerLink3 = p2p.Install(routers.Get(0), routers.Get(2)); // Link between router 0 and router 2

    // Step 3: Install internet stacks
    RipHelper ripRouting;

    InternetStackHelper internet;
    internet.SetRoutingHelper(ripRouting); // Use RIP as the routing protocol
    internet.Install(routers);

    // Step 4: Assign IP addresses to the links
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iRouter1 = ipv4.Assign(routerLink1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iRouter2 = ipv4.Assign(routerLink2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iRouter3 = ipv4.Assign(routerLink3);

    // Step 5: Enable Global Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Step 6: Add a UDP echo server on router 2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(2)); // Server on router 2
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Step 7: Add a UDP echo client on router 0
    UdpEchoClientHelper echoClient(iRouter2.GetAddress(1), 9); // Targeting router 2 IP
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(routers.Get(0)); // Client on router 0
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Step 8: Enable tracing (pcap files)
    p2p.EnablePcapAll("dvr-routing");

    // Step 9: Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

