#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DvrSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create router and subnet nodes
    NodeContainer routers;
    routers.Create(3); // R0, R1, R2

    NodeContainer subnetA, subnetB, subnetC, subnetD;
    subnetA.Create(1); // HostA
    subnetB.Create(1); // HostB
    subnetC.Create(1); // HostC
    subnetD.Create(1); // HostD

    // Connect subnets to routers
    NodeContainer r0subA(routers.Get(0), subnetA.Get(0));
    NodeContainer r0subB(routers.Get(0), subnetB.Get(0));
    NodeContainer r1subC(routers.Get(1), subnetC.Get(0));
    NodeContainer r2subD(routers.Get(2), subnetD.Get(0));

    // Create router interconnections
    NodeContainer r0r1(routers.Get(0), routers.Get(1));
    NodeContainer r1r2(routers.Get(1), routers.Get(2));
    NodeContainer r0r2(routers.Get(0), routers.Get(2));

    // Install Internet stack with RIP (Distance Vector Routing)
    RipHelper ripRouting;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(ripRouting, 0);
    InternetStackHelper internetRouters;
    internetRouters.SetRoutingHelper(listRH);
    internetRouters.Install(routers);

    // Subnet hosts use static routing
    InternetStackHelper internetHosts;
    internetHosts.Install(subnetA);
    internetHosts.Install(subnetB);
    internetHosts.Install(subnetC);
    internetHosts.Install(subnetD);

    // Set up PointToPoint links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create all NetDeviceContainers
    NetDeviceContainer dev_r0r1 = p2p.Install(r0r1);
    NetDeviceContainer dev_r1r2 = p2p.Install(r1r2);
    NetDeviceContainer dev_r0r2 = p2p.Install(r0r2);
    NetDeviceContainer dev_r0subA = p2p.Install(r0subA);
    NetDeviceContainer dev_r0subB = p2p.Install(r0subB);
    NetDeviceContainer dev_r1subC = p2p.Install(r1subC);
    NetDeviceContainer dev_r2subD = p2p.Install(r2subD);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0"); // R0-R1
    Ipv4InterfaceContainer if_r0r1 = ipv4.Assign(dev_r0r1);

    ipv4.SetBase("10.0.1.0", "255.255.255.0"); // R1-R2
    Ipv4InterfaceContainer if_r1r2 = ipv4.Assign(dev_r1r2);

    ipv4.SetBase("10.0.2.0", "255.255.255.0"); // R0-R2
    Ipv4InterfaceContainer if_r0r2 = ipv4.Assign(dev_r0r2);

    ipv4.SetBase("10.1.0.0", "255.255.255.0"); // subnetA <-> R0
    Ipv4InterfaceContainer if_r0subA = ipv4.Assign(dev_r0subA);

    ipv4.SetBase("10.2.0.0", "255.255.255.0"); // subnetB <-> R0
    Ipv4InterfaceContainer if_r0subB = ipv4.Assign(dev_r0subB);

    ipv4.SetBase("10.3.0.0", "255.255.255.0"); // subnetC <-> R1
    Ipv4InterfaceContainer if_r1subC = ipv4.Assign(dev_r1subC);

    ipv4.SetBase("10.4.0.0", "255.255.255.0"); // subnetD <-> R2
    Ipv4InterfaceContainer if_r2subD = ipv4.Assign(dev_r2subD);

    // Set up static routing for hosts (default gateway)
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> hostAStatic = staticRouting.GetStaticRouting(subnetA.Get(0)->GetObject<Ipv4>());
    hostAStatic->SetDefaultRoute(if_r0subA.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> hostBStatic = staticRouting.GetStaticRouting(subnetB.Get(0)->GetObject<Ipv4>());
    hostBStatic->SetDefaultRoute(if_r0subB.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> hostCStatic = staticRouting.GetStaticRouting(subnetC.Get(0)->GetObject<Ipv4>());
    hostCStatic->SetDefaultRoute(if_r1subC.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> hostDStatic = staticRouting.GetStaticRouting(subnetD.Get(0)->GetObject<Ipv4>());
    hostDStatic->SetDefaultRoute(if_r2subD.GetAddress(0), 1);

    // Enable RIP debugging (routing table changes)
    ripRouting.Set("Trace", BooleanValue(true));

    // Enable pcap tracing
    p2p.EnablePcapAll("dvr");

    // Set up application: HostA sends UDP to HostD
    uint16_t port = 50000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(subnetD.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpClientHelper client(if_r2subD.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(subnetA.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(18.0));

    // Animation (optional, does not affect simulation)
    AnimationInterface anim("dvr.xml");

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}