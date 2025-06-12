#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-ospf-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinkStateRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four routers
    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer router1_router2 = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    NetDeviceContainer router2_router3 = p2p.Install(NodeContainer(routers.Get(1), routers.Get(2)));
    NetDeviceContainer router3_router4 = p2p.Install(NodeContainer(routers.Get(2), routers.Get(3)));
    NetDeviceContainer router1_router3 = p2p.Install(NodeContainer(routers.Get(0), routers.Get(2)));

    // Install Internet stack with OSPF routing
    InternetStackHelper internet;
    Ipv4OspfRoutingHelper ospfRouting;
    internet.SetRoutingHelper(ospfRouting);
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer router1_router2_iface = ipv4.Assign(router1_router2);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer router2_router3_iface = ipv4.Assign(router2_router3);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer router3_router4_iface = ipv4.Assign(router3_router4);

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer router1_router3_iface = ipv4.Assign(router1_router3);

    // Create a host node connected to router 1
    NodeContainer host;
    host.Create(1);
    NetDeviceContainer host_router1 = p2p.Install(host, routers.Get(0));
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer host_iface = ipv4.Assign(host_router1);
    internet.Install(host);

    // Create another host connected to router 4
    NodeContainer host2;
    host2.Create(1);
    NetDeviceContainer router4_host2 = p2p.Install(routers.Get(3), host2);
    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer host2_iface = ipv4.Assign(router4_host2);
    internet.Install(host2);

    // Set up echo server on host2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(host2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up echo client from host to host2
    UdpEchoClientHelper echoClient(host2_iface.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(host.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing for LSA analysis
    p2p.EnablePcapAll("lsa_simulation");

    // Schedule a network change: break the link between router1 and router3 at 5 seconds
    Simulator::Schedule(Seconds(5.0), &PointToPointNetDevice::SetQueue, router1_router3.Get(0), PointerValue(CreateObject<DropTailQueue<Packet>>()));
    Simulator::Schedule(Seconds(5.0), &PointToPointNetDevice::SetQueue, router1_router3.Get(1), PointerValue(CreateObject<DropTailQueue<Packet>>()));

    // Resume the link after 2 seconds (at 7 seconds)
    Simulator::Schedule(Seconds(7.0), &Ipv4OspfRouting::LinkUp, DynamicCast<Ipv4OspfRouting>(ospfRouting.GetObject<Ipv4RoutingProtocol>()));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}