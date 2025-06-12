#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospfv2-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSquareTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between nodes in a square topology:
    // Node 0 <-> Node 1
    // Node 1 <-> Node 2
    // Node 2 <-> Node 3
    // Node 3 <-> Node 0

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices2_3 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices3_0 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack with OSPF routing
    InternetStackHelper stack;
    Ospfv2RoutingHelper ospfRouting;

    stack.SetRoutingProtocol(ospfRouting);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0_1 = address.Assign(devices0_1);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1_2 = address.Assign(devices1_2);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2_3 = address.Assign(devices2_3);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3_0 = address.Assign(devices3_0);

    // Enable routing tables logging
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Optional: Schedule printing of routing tables
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
        NS_LOG_INFO("Node " << i << " Routing Table:");
        rp->PrintRoutingTable(Ptr<OutputStreamWrapper>(Create<OutputStreamWrapper>(&std::cout)));
    }

    // Add echo server on node 2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Add echo client on node 0, sending to node 2's IP
    UdpEchoClientHelper echoClient(interfaces1_2.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}