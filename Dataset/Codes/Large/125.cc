#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipExample");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Step 1: Create nodes
    NodeContainer centralRouter;
    centralRouter.Create(1); // The central router

    NodeContainer edgeRouters;
    edgeRouters.Create(4); // 4 edge routers

    // Step 2: Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links between central router and each edge router
    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(centralRouter.Get(0), edgeRouters.Get(i));
    }

    // Step 3: Install internet stacks
    RipHelper ripRouting;

    InternetStackHelper internet;
    internet.SetRoutingHelper(ripRouting); // Set RIP as the routing protocol
    internet.Install(centralRouter);
    internet.Install(edgeRouters);

    // Step 4: Assign IP addresses
    Ipv4AddressHelper ipv4;

    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = ipv4.Assign(devices[i]);
    }

    // Step 5: Set up routing information protocol (RIP)
    for (int i = 0; i < 4; ++i)
    {
        ripRouting.AddNetworkRouteTo(centralRouter.Get(0), interfaces[i].GetAddress(0), interfaces[i].GetAddress(1), 1);
    }

    // Step 6: Add a simple UDP Echo application
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(edgeRouters.Get(0)); // Install the server on the first edge router
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(edgeRouters.Get(1)); // Install the client on the second edge router
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Step 7: Enable tracing (pcap trace)
    p2p.EnablePcapAll("rip-routing");

    // Step 8: Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

