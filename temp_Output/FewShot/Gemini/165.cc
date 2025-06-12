#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ospf-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper internet;
    OspfHelper ospf;
    internet.SetRoutingHelper(ospf);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[4];
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Optional: Print routing tables
    for (int i = 0; i < 4; ++i) {
        std::cout << "Routing table for Node " << i << ":" << std::endl;
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("ospf-routing-" + std::to_string(i) + ".routes", std::ios::out);
        ospf.PrintRoutingTableAllAt(Seconds(10.0), nodes.Get(i)->GetObject<Ipv4L3Protocol>(), routingStream);
    }


    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}