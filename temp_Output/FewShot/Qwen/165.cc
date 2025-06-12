#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for OSPF and routing
    LogComponentEnable("Ospf", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);

    // Create 4 nodes arranged in a square topology
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between each adjacent node (forming a square)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stack on all nodes
    InternetStackHelper stack;

    // Install OSPF routing helper
    OspfHelper ospfRouting;
    stack.SetRoutingHelper(ospfRouting); // Assign OSPF as the routing protocol
    stack.Install(nodes);

    // Create links: node0 <-> node1, node1 <-> node2, node2 <-> node3, node3 <-> node0
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Assign IP addresses to interfaces
    Ipv4AddressHelper address;

    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces01 = address.Assign(devices01);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces12 = address.Assign(devices12);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces23 = address.Assign(devices23);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces30 = address.Assign(devices30);

    // Set up ping application to monitor connectivity and indirectly verify routes
    V4PingHelper ping(nodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ping.SetAttribute("Verbose", EnumValue(V4Ping::VERBOSE));
    ApplicationContainer apps = ping.Install(nodes.Get(2));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    // Schedule printing of routing tables at each node
    Ptr<OutputStreamWrapper> routingStream = CreateFileStream("ospf-routing-tables.txt");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        std::ostringstream oss;
        oss << "/NodeList/" << i << "/$ns3::Ipv4L3Protocol/RoutingTable";
        Config::ConnectWithoutContext(oss.str(), MakeBoundCallback(&Ipv4RoutingHelper::PrintStaticRoutingTable, routingStream));
    }

    // Run simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}