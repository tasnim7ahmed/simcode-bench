#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include "ns3/ospf-module.h"

using namespace ns3;

void PrintRoutingTable (Ptr<Node> node, Time printTime)
{
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    std::ostringstream oss;
    ipv4->GetRoutingProtocol ()->PrintRoutingTable (&oss);
    std::cout << "Routing table for node " << node->GetId()
              << " at time " << printTime.GetSeconds() << "s\n";
    std::cout << oss.str() << std::endl;
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(4);

    // Square topology node arrangement:
    // 0---1
    // |   |
    // 3---2

    // Create links for the square topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack with OSPF routing
    OspfHelper ospfRouting;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospfRouting);
    stack.Install(nodes);

    // Assign IP addresses to the links
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer i01, i12, i23, i30;

    address.SetBase("10.0.1.0", "255.255.255.0");
    i01 = address.Assign(d01);

    address.SetBase("10.0.2.0", "255.255.255.0");
    i12 = address.Assign(d12);

    address.SetBase("10.0.3.0", "255.255.255.0");
    i23 = address.Assign(d23);

    address.SetBase("10.0.4.0", "255.255.255.0");
    i30 = address.Assign(d30);

    // Enable global routing table population for OSPF simulation
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Print routing tables after OSPF convergence
    Simulator::Schedule(Seconds(5.0), &PrintRoutingTable, nodes.Get(0), Seconds(5.0));
    Simulator::Schedule(Seconds(5.0), &PrintRoutingTable, nodes.Get(1), Seconds(5.0));
    Simulator::Schedule(Seconds(5.0), &PrintRoutingTable, nodes.Get(2), Seconds(5.0));
    Simulator::Schedule(Seconds(5.0), &PrintRoutingTable, nodes.Get(3), Seconds(5.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}