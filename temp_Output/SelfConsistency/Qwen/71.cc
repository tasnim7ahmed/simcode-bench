#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterGlobalRoutingScenario");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes A, B, and C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link A <-> B
    NetDeviceContainer devAB;
    devAB = p2p.Install(nodeA, nodeB);

    // Link B <-> C
    NetDeviceContainer devBC;
    devBC = p2p.Install(nodeB, nodeC);

    // Install Internet stack with global routing
    InternetStackHelper stack;
    stack.SetRoutingHelper(Ipv4GlobalRoutingHelper()); // Set global routing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Subnets for point-to-point links
    address.SetBase("10.1.1.0", "255.255.255.252"); // x.x.x.0/30
    Ipv4InterfaceContainer ifAB = address.Assign(devAB);

    address.SetBase("10.2.2.0", "255.255.255.252"); // y.y.y.0/30
    Ipv4InterfaceContainer ifBC = address.Assign(devBC);

    // Assign /32 IP addresses to loopback interfaces on A and C
    Ipv4InterfaceContainer loopbackIfsA, loopbackIfsC;

    {
        Ipv4Address loopbackAddrA("172.16.1.1");
        Ipv4InterfaceContainer loopIfs = nodeA->GetObject<Ipv4>()->AddInterface(Ptr<NetDevice>(nullptr));
        Ipv4AddressHelper::NewNetworkPrefix(loopIfs, loopbackAddrA, "255.255.255.255");
        loopbackIfsA.Add(loopIfs);
    }

    {
        Ipv4Address loopbackAddrC("192.168.1.1");
        Ipv4InterfaceContainer loopIfs = nodeC->GetObject<Ipv4>()->AddInterface(Ptr<NetDevice>(nullptr));
        Ipv4AddressHelper::NewNetworkPrefix(loopIfs, loopbackAddrC, "255.255.255.255");
        loopbackIfsC.Add(loopIfs);
    }

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP traffic from A to C using loopback addresses
    uint16_t port = 9; // Discard port (standard)

    // Packet sink on C
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(loopbackIfsC.GetAddress(0), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodeC);
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // On-demand UDP echo client on A
    OnOffHelper clientHelper("ns3::UdpSocketFactory",
                             InetSocketAddress(loopbackIfsC.GetAddress(0), port));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = clientHelper.Install(nodeA);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-global-routing.tr"));
    p2p.EnablePcapAll("three-router-global-routing");

    // Animation output
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Create animation file
    AnimationInterface anim("three-router-global-routing.xml");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}