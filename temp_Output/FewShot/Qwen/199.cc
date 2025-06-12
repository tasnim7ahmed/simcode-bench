#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time and create nodes
    double simTime = 10.0;
    NodeContainer r1r2, r2r3, r2lan;
    NodeContainer routers;
    NodeContainer lanNodes;
    NodeContainer srcNode, dstNode;

    Ptr<Node> R1 = CreateObject<Node>();
    Ptr<Node> R2 = CreateObject<Node>();
    Ptr<Node> R3 = CreateObject<Node>();

    routers.Add(R1);
    routers.Add(R2);
    routers.Add(R3);

    // Create point-to-point links between routers
    PointToPointHelper p2pR1R2;
    p2pR1R2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pR1R2.SetChannelAttribute("Delay", StringValue("10ms"));
    r1r2 = p2pR1R2.Install(R1, R2);

    PointToPointHelper p2pR2R3;
    p2pR2R3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pR2R3.SetChannelAttribute("Delay", StringValue("10ms"));
    r2r3 = p2pR2R3.Install(R2, R3);

    // Create LAN on R2 using CSMA
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    lanNodes.Create(3);  // LAN nodes: source node + 2 others
    srcNode.Add(lanNodes.Get(0));
    NodeContainer lanOthers = NodeContainer(lanNodes.Get(1), lanNodes.Get(2));

    NetDeviceContainer lanDevices = csma.Install(lanNodes);
    NetDeviceContainer r2lanDev = csma.Install(R2, lanNodes.Get(0));  // Connect R2 to LAN (dummy link)

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper addr;

    addr.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer r1r2Ifaces = addr.Assign(r1r2);

    addr.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer r2r3Ifaces = addr.Assign(r2r3);

    addr.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer lanIfaces = addr.Assign(lanDevices);

    // Manually assign an IP to R2's LAN interface (already assigned via lanDevices above)

    // Set up default routes for the routers
    Ipv4StaticRoutingHelper ipv4RoutingHelper;

    // Route from R1 to R2 and beyond
    Ptr<Ipv4StaticRouting> r1Routing = ipv4RoutingHelper.GetStaticRouting(R1->GetObject<Ipv4>());
    r1Routing->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), 2);
    r1Routing->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), 2);

    // Route from R3 to R2 and beyond
    Ptr<Ipv4StaticRouting> r3Routing = ipv4RoutingHelper.GetStaticRouting(R3->GetObject<Ipv4>());
    r3Routing->AddNetworkRouteTo(Ipv4Address("10.0.0.0"), Ipv4Mask("255.255.255.0"), 1);
    r3Routing->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), 1);

    // Enable global routing so that routers know about each other
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP sink at destination (on R3)
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(r2r3Ifaces.GetAddress(1), sinkPort)); // R3's address

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(R3);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // Setup OnOff application on source node in LAN to send traffic to R3
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite

    ApplicationContainer clientApp = clientHelper.Install(srcNode);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    // Setup FlowMonitor to collect data
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Write flow monitor results to file
    flowmon.SerializeToXmlFile("tcp-throughput.xml", true, true);

    Simulator::Destroy();
    return 0;
}