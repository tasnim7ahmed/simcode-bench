#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospfv2-routing-protocol.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSquareTopology");

int main(int argc, char *argv[]) {
    bool verbose = true;
    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create links between nodes in a square: 0 <-> 1 <-> 2 <-> 3 <-> 0
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer dev30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper internet;
    Ipv4ListRoutingHelper listRH;

    // OSPF routing protocol
    Ospfv2Helper ospfRoutingHelper;
    listRH.Add(ospfRoutingHelper, 10);

    internet.SetRoutingHelper(listRH);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ipv4.Assign(dev01);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ipv4.Assign(dev12);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    ipv4.Assign(dev23);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ipv4.Assign(dev30);

    // Print routing tables every 1 second
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Schedule(Seconds(1.0), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(1.0), nodes);
    Simulator::Schedule(Seconds(2.0), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(2.0), nodes);
    Simulator::Schedule(Seconds(3.0), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(3.0), nodes);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}