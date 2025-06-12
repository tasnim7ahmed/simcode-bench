#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodesA, nodesB, nodesC;
    Ptr<Node> routerA = CreateObject<Node>();
    Ptr<Node> routerB = CreateObject<Node>();
    Ptr<Node> routerC = CreateObject<Node>();

    nodesA.Add(routerA);
    nodesB.Add(routerB);
    nodesC.Add(routerC);

    PointToPointHelper p2pAB;
    p2pAB.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pAB.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pBC;
    p2pBC.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBC.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = p2pAB.Install(routerA, routerB);
    NetDeviceContainer devicesBC = p2pBC.Install(routerB, routerC);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("1.1.1.0", "255.255.255.252"); // /30 subnet for A-B link
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("2.2.2.0", "255.255.255.252"); // /30 subnet for B-C link
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    // Assign a loopback address to Router A and C
    Ipv4Address routerALoopback("10.0.0.1");
    Ipv4Address routerCLoopback("20.0.0.1");

    Ipv4InterfaceContainer loopbackA, loopbackC;

    {
        Ptr<Ipv4> ipv4A = routerA->GetObject<Ipv4>();
        int32_t loopbackIndexA = ipv4A->AddInterface(Ptr<NetDevice>(nullptr));
        ipv4A->SetMetric(loopbackIndexA, 1);
        ipv4A->SetUp(loopbackIndexA);
        ipv4A->AddAddress(loopbackIndexA, Ipv4InterfaceAddress(routerALoopback, Ipv4Mask::Get32()));
        loopbackA.Add(ipv4A->GetAddress(loopbackIndexA, 0));
    }

    {
        Ptr<Ipv4> ipv4C = routerC->GetObject<Ipv4>();
        int32_t loopbackIndexC = ipv4C->AddInterface(Ptr<NetDevice>(nullptr));
        ipv4C->SetMetric(loopbackIndexC, 1);
        ipv4C->SetUp(loopbackIndexC);
        ipv4C->AddAddress(loopbackIndexC, Ipv4InterfaceAddress(routerCLoopback, Ipv4Mask::Get32()));
        loopbackC.Add(ipv4C->GetAddress(loopbackIndexC, 0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(routerC);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(loopbackC.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(routerA);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2pAB.EnableAsciiAll(ascii.CreateFileStream("three-router-topology.tr"));
    p2pBC.EnableAsciiAll(ascii.CreateFileStream("three-router-topology.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}