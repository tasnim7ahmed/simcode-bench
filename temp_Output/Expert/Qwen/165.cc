#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSquareTopology");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("OspfSquareTopology", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create links between nodes (forming a square: 0-1-2-3-0)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1)); // 0-1
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2)); // 1-2
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3)); // 2-3
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0)); // 3-0

    InternetStackHelper stack;
    OspfHelper ospfRouting;
    stack.SetRoutingHelper(ospfRouting); // Assign OSPF routing helper
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    address.SetBase("10.0.0.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.SetBase("10.0.1.0", "255.255.255.0");
    interfaces[1] = address.Assign(devices[1]);
    address.SetBase("10.0.2.0", "255.255.255.0");
    interfaces[2] = address.Assign(devices[2]);
    address.SetBase("10.0.3.0", "255.255.255.0");
    interfaces[3] = address.Assign(devices[3]);

    // Enable routing tables logging
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("ospf.routes", std::ios::out);
        ospfRouting.PrintRoutingTableAt(Seconds(2.0), nodes.Get(i), routingStream);
    }

    // Set up ping application to test connectivity
    V4PingHelper ping(nodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ApplicationContainer apps = ping.Install(nodes.Get(2));
    apps.Start(Seconds(3.0));
    apps.Stop(Seconds(6.0));

    Simulator::Stop(Seconds(7.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}