#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/rip-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("RipSimpleRouting", LOG_LEVEL_INFO);

    Time::SetResolution(Time::NS);
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer routers;
    routers.Create(3);

    NodeContainer subnet1; // Hosts on subnet1 - connected to R1
    subnet1.Create(1);
    NodeContainer subnet2; // Hosts on subnet2 - connected to R2
    subnet2.Create(1);
    NodeContainer subnet3; // Hosts on subnet3 - connected to R3
    subnet3.Create(1);
    NodeContainer subnet4; // Hosts on subnet4 - connected to R2
    subnet4.Create(1);

    // Point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect subnet1 <-> R1
    NodeContainer n1r1(subnet1.Get(0), routers.Get(0));
    NetDeviceContainer ndc1 = p2p.Install(n1r1);

    // Connect subnet2 <-> R2
    NodeContainer n2r2(subnet2.Get(0), routers.Get(1));
    NetDeviceContainer ndc2 = p2p.Install(n2r2);

    // Connect subnet3 <-> R3
    NodeContainer n3r3(subnet3.Get(0), routers.Get(2));
    NetDeviceContainer ndc3 = p2p.Install(n3r3);

    // Connect subnet4 <-> R2
    NodeContainer n4r2(subnet4.Get(0), routers.Get(1));
    NetDeviceContainer ndc4 = p2p.Install(n4r2);

    // Inter-router connections: R1-R2, R2-R3
    NodeContainer r1r2(routers.Get(0), routers.Get(1));
    NetDeviceContainer ndc5 = p2p.Install(r1r2);
    NodeContainer r2r3(routers.Get(1), routers.Get(2));
    NetDeviceContainer ndc6 = p2p.Install(r2r3);

    // IP address assignment
    InternetStackHelper stack;
    RipHelper ripRouting;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(ripRouting, 0);
    stack.SetRoutingHelper(listRH);
    stack.Install(routers);

    // Hosts use default global routing
    InternetStackHelper hostStack;
    Ipv4StaticRoutingHelper staticRh;
    Ipv4GlobalRoutingHelper globalRh;
    hostStack.SetRoutingHelper(globalRh);
    hostStack.Install(subnet1);
    hostStack.Install(subnet2);
    hostStack.Install(subnet3);
    hostStack.Install(subnet4);

    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> ifaces;

    // subnet1 <-> R1
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(ndc1));

    // subnet2 <-> R2
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(ndc2));

    // subnet3 <-> R3
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(ndc3));

    // subnet4 <-> R2
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(ndc4));

    // R1 <-> R2
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(ndc5));

    // R2 <-> R3
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ifaces.push_back(ipv4.Assign(ndc6));

    // Set up default routes on hosts
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4> ipv4Host;
    Ptr<Ipv4StaticRouting> hostStaticRouting;
    // subnet1 host: gateway R1 (10.0.1.2)
    ipv4Host = subnet1.Get(0)->GetObject<Ipv4>();
    hostStaticRouting = staticRouting.GetStaticRouting(ipv4Host);
    hostStaticRouting->SetDefaultRoute("10.0.1.2", 1);

    // subnet2 host: gateway R2 (10.0.2.2)
    ipv4Host = subnet2.Get(0)->GetObject<Ipv4>();
    hostStaticRouting = staticRouting.GetStaticRouting(ipv4Host);
    hostStaticRouting->SetDefaultRoute("10.0.2.2", 1);

    // subnet3 host: gateway R3 (10.0.3.2)
    ipv4Host = subnet3.Get(0)->GetObject<Ipv4>();
    hostStaticRouting = staticRouting.GetStaticRouting(ipv4Host);
    hostStaticRouting->SetDefaultRoute("10.0.3.2", 1);

    // subnet4 host: gateway R2 (10.0.4.2)
    ipv4Host = subnet4.Get(0)->GetObject<Ipv4>();
    hostStaticRouting = staticRouting.GetStaticRouting(ipv4Host);
    hostStaticRouting->SetDefaultRoute("10.0.4.2", 1);

    // Enable pcap tracing on all point-to-point devices
    p2p.EnablePcapAll("dvr-example", true);

    // Create traffic: UDP echo from subnet1 host to subnet4 host (through routers)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(subnet4.Get(0));
    serverApps.Start(Seconds(4.0));
    serverApps.Stop(Seconds(24.0));

    UdpEchoClientHelper echoClient(ifaces[3].GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(6));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(subnet1.Get(0));
    clientApps.Start(Seconds(8.0));
    clientApps.Stop(Seconds(24.0));

    // Enable routing table logging
    RipHelper::PrintRoutingTableAllAt(Seconds(6.0), routers);
    RipHelper::PrintRoutingTableAllAt(Seconds(12.0), routers);
    RipHelper::PrintRoutingTableAllAt(Seconds(20.0), routers);

    Simulator::Stop(Seconds(24.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}