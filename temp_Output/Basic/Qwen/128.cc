#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinkStateRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create routers
    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[4];

    // Connect routers in a square topology: 0-1, 1-2, 2-3, 3-0
    routerDevices[0] = p2p.Install(routers.Get(0), routers.Get(1));
    routerDevices[1] = p2p.Install(routers.Get(1), routers.Get(2));
    routerDevices[2] = p2p.Install(routers.Get(2), routers.Get(3));
    routerDevices[3] = p2p.Install(routers.Get(3), routers.Get(0));

    // Install Internet stack with OSPF routing
    InternetStackHelper internet;
    OspfHelper ospfRouting;
    ospfRouting.PopulateRoutingTables();

    internet.SetRoutingHelper(ospfRouting);
    internet.Install(routers);

    // Assign IP addresses to router interfaces
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer routerInterfaces[4];

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    routerInterfaces[0] = ipv4.Assign(routerDevices[0]);
    ipv4.NewNetwork();
    routerInterfaces[1] = ipv4.Assign(routerDevices[1]);
    ipv4.NewNetwork();
    routerInterfaces[2] = ipv4.Assign(routerDevices[2]);
    ipv4.NewNetwork();
    routerInterfaces[3] = ipv4.Assign(routerDevices[3]);

    // Add a host node connected to Router 0
    Ptr<Node> host = CreateObject<Node>();
    NodeContainer hostNode(host);
    internet.Install(hostNode);

    NetDeviceContainer hostRouterLink = p2p.Install(host, routers.Get(0));
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer hostInterface = ipv4.Assign(hostRouterLink);

    // Create application: UdpEcho from host to Router 2
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(routers.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(routerInterfaces[1].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(host);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("lsa_simulation");

    // Simulate link failure between Router 0 and 1 at time 4s
    Simulator::Schedule(Seconds(4.0), &PointToPointNetDevice::SetLinkStatus, routers.Get(0)->GetDevice(0), false);
    // Restore the link at time 6s
    Simulator::Schedule(Seconds(6.0), &PointToPointNetDevice::SetLinkStatus, routers.Get(0)->GetDevice(0), true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}