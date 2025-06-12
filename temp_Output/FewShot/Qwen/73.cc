#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create three nodes (routers A, B, C)
    NodeContainer routers;
    routers.Create(3);

    // Point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(routers);

    // Assign IP addresses to interfaces
    Ipv4AddressHelper addrAB, addrBC, addrA, addrC;

    // Link A-B subnet x.x.x.0/30
    addrAB.SetBase("192.168.1.0", "255.255.255.252");
    NetDeviceContainer devAB = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    Ipv4InterfaceContainer ipAB = addrAB.Assign(devAB);

    // Link B-C subnet y.y.y.0/30
    addrBC.SetBase("192.168.2.0", "255.255.255.252");
    NetDeviceContainer devBC = p2p.Install(NodeContainer(routers.Get(1), routers.Get(2)));
    Ipv4InterfaceContainer ipBC = addrBC.Assign(devBC);

    // Assign /32 IP addresses to loopback-like interfaces on Router A and C
    Ptr<Node> routerA = routers.Get(0);
    Ptr<Ipv4> ipv4A = routerA->GetObject<Ipv4>();
    uint32_t loopbackA = ipv4A->AddInterface();
    Ipv4InterfaceAddress iaddrA("1.1.1.1", "255.255.255.255");
    ipv4A->AddAddress(loopbackA, iaddrA);
    ipv4A->SetMetric(loopbackA, 1);
    ipv4A->SetUp(loopbackA);

    Ptr<Node> routerC = routers.Get(2);
    Ptr<Ipv4> ipv4C = routerC->GetObject<Ipv4>();
    uint32_t loopbackC = ipv4C->AddInterface();
    Ipv4InterfaceAddress iaddrC("3.3.3.3", "255.255.255.255");
    ipv4C->AddAddress(loopbackC, iaddrC);
    ipv4C->SetMetric(loopbackC, 1);
    ipv4C->SetUp(loopbackC);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup OnOff UDP application from Router A to Router C
    uint16_t port = 9; // UDP port

    // Packet sink on Router C
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address("3.3.3.3"), port));
    ApplicationContainer sinkApp = sink.Install(routers.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // OnOff application on Router A
    OnOffHelper client("ns3::UdpSocketFactory",
                       InetSocketAddress(Ipv4Address("3.3.3.3"), port));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    client.SetAttribute("PacketSize", UintegerValue(512));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = client.Install(routers.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable packet tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("router-network.tr"));
    pointToPoint.EnablePcapAll("router-network");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}