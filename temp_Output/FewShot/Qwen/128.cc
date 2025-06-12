#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-ospf-routing.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging for OSPF and routing
    LogComponentEnable("Ipv4OspfRouting", LOG_LEVEL_INFO);
    LogComponentEnable("OspfLinkStateAdvert", LOG_LEVEL_INFO);

    // Create four nodes representing routers
    NodeContainer routers;
    routers.Create(4);

    // Setup point-to-point links between router 0 <-> 1, 1 <-> 2, 1 <-> 3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev01 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer dev12 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer dev13 = p2p.Install(routers.Get(1), routers.Get(3));

    // Install internet stack with OSPF routing
    InternetStackHelper stack;
    Ipv4OspfRoutingHelper ospfRouting;

    stack.SetRoutingHelper(ospfRouting); // Use OSPF as the routing protocol
    stack.Install(routers);

    // Assign IP addresses to interfaces
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign(dev01);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address.Assign(dev12);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if13 = address.Assign(dev13);

    // Enable pcap tracing on all devices to visualize LSA exchanges
    p2p.EnablePcapAll("ospf-lsa");

    // Create a UDP echo server on router 2 (node 2)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Create a UDP echo client on router 0 (node 0) sending traffic to router 2
    UdpEchoClientHelper echoClient(if12.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(routers.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Simulate link failure between router 1 and router 2 at time 10s
    Simulator::Schedule(Seconds(10.0), &Ipv4::SetDown, routers.Get(1)->GetObject<Ipv4>(), if12.GetInterfaceIndex(0));
    Simulator::Schedule(Seconds(10.0), &Ipv4::SetDown, routers.Get(2)->GetObject<Ipv4>(), if12.GetInterfaceIndex(1));

    // Resume link at time 15s
    Simulator::Schedule(Seconds(15.0), &Ipv4::SetUp, routers.Get(1)->GetObject<Ipv4>(), if12.GetInterfaceIndex(0));
    Simulator::Schedule(Seconds(15.0), &Ipv4::SetUp, routers.Get(2)->GetObject<Ipv4>(), if12.GetInterfaceIndex(1));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}