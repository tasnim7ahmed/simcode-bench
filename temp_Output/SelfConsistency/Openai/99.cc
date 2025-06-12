#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterGlobalRoutingExample");

int
main(int argc, char *argv[])
{
    LogComponentEnable("ThreeRouterGlobalRoutingExample", LOG_LEVEL_INFO);
    // Step 1: Create Nodes
    Ptr<Node> nodeA = CreateObject<Node>();
    Ptr<Node> nodeB = CreateObject<Node>();
    Ptr<Node> nodeC = CreateObject<Node>();

    NodeContainer nodesAB (nodeA, nodeB);
    NodeContainer nodesBC (nodeB, nodeC);

    // Step 2: Install Internet stack
    InternetStackHelper internet;
    internet.Install(NodeContainer(nodeA, nodeB, nodeC));

    // Step 3: Create Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = p2p.Install(nodesAB);
    NetDeviceContainer devicesBC = p2p.Install(nodesBC);

    // Step 4: Assign IP Addresses
    Ipv4AddressHelper ipv4;

    // A-B link: 10.0.1.0/30
    ipv4.SetBase("10.0.1.0", "255.255.255.252");
    Ipv4InterfaceContainer ifacesAB = ipv4.Assign(devicesAB);

    // B-C link: 10.0.2.0/30
    ipv4.SetBase("10.0.2.0", "255.255.255.252");
    Ipv4InterfaceContainer ifacesBC = ipv4.Assign(devicesBC);

    // Step 5: Assign /32 loopbacks for A and C
    // (simulate host routes)
    Ipv4Address loopA("1.1.1.1");
    Ipv4Address loopC("3.3.3.3");

    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();

    int32_t ifA = ipv4A->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4A->AddAddress(ifA, Ipv4InterfaceAddress(loopA, Ipv4Mask("255.255.255.255")));
    ipv4A->SetMetric(ifA, 1);
    ipv4A->SetUp(ifA);

    int32_t ifC = ipv4C->AddInterface(CreateObject<LoopbackNetDevice>());
    ipv4C->AddAddress(ifC, Ipv4InterfaceAddress(loopC, Ipv4Mask("255.255.255.255")));
    ipv4C->SetMetric(ifC, 1);
    ipv4C->SetUp(ifC);

    // Step 6: Configure Static Host Routes and Global Routing
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingA = staticRoutingHelper.GetStaticRouting(ipv4A);
    // Add default route at A via B
    staticRoutingA->SetDefaultRoute(ifacesAB.GetAddress(1), 1);

    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRoutingB = staticRoutingHelper.GetStaticRouting(ipv4B);

    // Add host route to loopA via AB interface
    staticRoutingB->AddHostRouteTo(loopA, ifacesAB.GetAddress(0), 1);
    // Add host route to loopC via BC interface
    staticRoutingB->AddHostRouteTo(loopC, ifacesBC.GetAddress(1), 2);

    Ptr<Ipv4StaticRouting> staticRoutingC = staticRoutingHelper.GetStaticRouting(ipv4C);
    // Add default route at C via B
    staticRoutingC->SetDefaultRoute(ifacesBC.GetAddress(0), 1);

    // Step 7: Enable Global Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Step 8: Applications
    // C: UDP server (sink) on port 9999, listening on loopback address
    uint16_t port = 9999;
    UdpServerHelper server(port);
    ApplicationContainer appsS = server.Install(nodeC);
    appsS.Start(Seconds(0.5));
    appsS.Stop(Seconds(5.0));

    // A: OnOff UDP client sending to C's loopback address
    OnOffHelper client("ns3::UdpSocketFactory",
                       InetSocketAddress(loopC, port));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(512));
    client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    client.SetAttribute("StopTime", TimeValue(Seconds(4.5)));
    client.SetAttribute("Remote", AddressValue(InetSocketAddress(loopC, port)));

    ApplicationContainer appsC = client.Install(nodeA);

    // Step 9: Enable PCAP and tracing
    p2p.EnablePcapAll("three-router");

    // ASCII trace, optional
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    // Step 10: Run
    Simulator::Stop(Seconds(6.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}