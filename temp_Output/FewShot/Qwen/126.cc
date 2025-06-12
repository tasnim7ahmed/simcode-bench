#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for Distance Vector routing
    LogComponentEnable("DvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes: 3 routers and 4 end hosts (for subnetworks)
    NodeContainer hosts;
    hosts.Create(4);

    NodeContainer routers;
    routers.Create(3);

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Router connections:
    // R1 <-> R2
    // R2 <-> R3
    // Each router connected to a host
    NetDeviceContainer r1r2 = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    NetDeviceContainer r2r3 = p2p.Install(NodeContainer(routers.Get(1), routers.Get(2)));

    // Connect each host to one router
    NetDeviceContainer h1r1 = p2p.Install(NodeContainer(hosts.Get(0), routers.Get(0)));
    NetDeviceContainer h2r1 = p2p.Install(NodeContainer(hosts.Get(1), routers.Get(0)));
    NetDeviceContainer h3r3 = p2p.Install(NodeContainer(hosts.Get(2), routers.Get(2)));
    NetDeviceContainer h4r3 = p2p.Install(NodeContainer(hosts.Get(3), routers.Get(2)));

    // Install internet stack on all nodes
    InternetStackHelper stack;

    // Install DV routing on routers
    DvRoutingHelper dvRouting;
    stack.SetRoutingHelper(dvRouting);
    stack.Install(routers);

    // Install regular internet stack on hosts
    InternetStackHelper().Install(hosts);

    // Assign IP addresses
    Ipv4AddressHelper ip;

    ip.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1r2 = ip.Assign(r1r2);

    ip.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r2r3 = ip.Assign(r2r3);

    ip.SetBase("10.0.0.0", "255.255.255.0");
    ip.Assign(h1r1);

    ip.SetBase("10.0.1.0", "255.255.255.0");
    ip.Assign(h2r1);

    ip.SetBase("10.0.2.0", "255.255.255.0");
    ip.Assign(h3r3);

    ip.SetBase("10.0.3.0", "255.255.255.0");
    ip.Assign(h4r3);

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("dv-routing");

    // Set up UDP Echo Server on host 3 (reachable via R3)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(hosts.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP Echo Client on host 0 (reachable via R1)
    Ipv4Address remoteAddr = Ipv4Address("10.0.2.1"); // Host 3's IP on its subnet
    UdpEchoClientHelper echoClient(remoteAddr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(hosts.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}