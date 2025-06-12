#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DVRSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 routers and 4 subnetwork hosts
    NodeContainer routers;
    routers.Create(3);

    NodeContainer hosts;
    hosts.Create(4);

    // Point-to-point links between routers (forming a triangle)
    PointToPointHelper p2pRouter1Router2;
    p2pRouter1Router2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRouter1Router2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pRouter2Router3;
    p2pRouter2Router3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRouter2Router3.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pRouter3Router1;
    p2pRouter3Router1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRouter3Router1.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[3];
    routerDevices[0] = p2pRouter1Router2.Install(routers.Get(0), routers.Get(1));
    routerDevices[1] = p2pRouter2Router3.Install(routers.Get(1), routers.Get(2));
    routerDevices[2] = p2pRouter3Router1.Install(routers.Get(2), routers.Get(0));

    // Connect each host to one of the routers
    PointToPointHelper p2pHostToRouter;
    p2pHostToRouter.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2pHostToRouter.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer hostDevices[4];
    hostDevices[0] = p2pHostToRouter.Install(hosts.Get(0), routers.Get(0));
    hostDevices[1] = p2pHostToRouter.Install(hosts.Get(1), routers.Get(1));
    hostDevices[2] = p2pHostToRouter.Install(hosts.Get(2), routers.Get(2));
    hostDevices[3] = p2pHostToRouter.Install(hosts.Get(3), routers.Get(0)); // Another host on first router

    // Install Internet stack with DVR routing
    InternetStackHelper internet;
    DvRoutingHelper dvRouting;
    internet.SetRoutingHelper(dvRouting); // has effect on the next Install

    internet.Install(routers);
    internet.Install(hosts);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces[3];
    routerInterfaces[0] = ipv4.Assign(routerDevices[0]);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    routerInterfaces[1] = ipv4.Assign(routerDevices[1]);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    routerInterfaces[2] = ipv4.Assign(routerDevices[2]);

    Ipv4InterfaceContainer hostInterfaces[4];
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    hostInterfaces[0] = ipv4.Assign(hostDevices[0]);

    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    hostInterfaces[1] = ipv4.Assign(hostDevices[1]);

    ipv4.SetBase("192.168.3.0", "255.255.255.0");
    hostInterfaces[2] = ipv4.Assign(hostDevices[2]);

    ipv4.SetBase("192.168.4.0", "255.255.255.0");
    hostInterfaces[3] = ipv4.Assign(hostDevices[3]);

    // Enable pcap tracing on all devices
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream oss;
        oss << "router-" << i + 1 << "-device";
        p2pRouter1Router2.EnablePcapAll(oss.str().c_str());
    }

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream oss;
        oss << "host-" << i + 1 << "-device";
        p2pHostToRouter.EnablePcapAll(oss.str().c_str());
    }

    // Set up applications: UDP echo server on host 3 (IP: 192.168.3.1)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client from host 0 to host 3
    UdpEchoClientHelper echoClient(hostInterfaces[2].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(hosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}