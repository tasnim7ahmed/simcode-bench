#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (four routers)
    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer link0 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer link1 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer link2 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer link3 = p2p.Install(routers.Get(0), routers.Get(3));

    // Install Internet stack with OSPF routing
    InternetStackHelper stack;
    Ipv4OspfRoutingHelper ospfRouting;
    stack.SetRoutingHelper(ospfRouting);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i0 = address.Assign(link0);
    address.NewNetwork();
    Ipv4InterfaceContainer i1 = address.Assign(link1);
    address.NewNetwork();
    Ipv4InterfaceContainer i2 = address.Assign(link2);
    address.NewNetwork();
    Ipv4InterfaceContainer i3 = address.Assign(link3);

    // Enable PCAP tracing
    p2p.EnablePcapAll("ospf-simulation");

    // Setup traffic: Echo server on node 3, client from node 0
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(routers.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i3.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(routers.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Simulation run
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}