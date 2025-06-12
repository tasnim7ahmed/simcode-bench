#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinitySimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    double simulationTime = 20.0;

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes for routers A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Define point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create node containers for the links
    NodeContainer linkAB = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer linkBC = NodeContainer(nodes.Get(1), nodes.Get(2));

    NetDeviceContainer devicesAB = p2p.Install(linkAB);
    NetDeviceContainer devicesBC = p2p.Install(linkBC);

    // Install Internet stack with Distance Vector routing
    InternetStackHelper stack;

    DvRoutingHelper dv;
    Ipv4ListRoutingHelper list;
    list.Add(dv, 0); // priority 0

    stack.SetRoutingHelper(list); // has to be set before installing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    // Enable routing tables output
    Ptr<OutputStreamWrapper> routingStream = CreateFileStreamWrapper("dv-routing-tables.nodes", std::ios::out);
    dv.PrintRoutingTableAllAt(Seconds(2), routingStream);
    dv.PrintRoutingTableAllEvery(Seconds(1), routingStream, Seconds(2), Seconds(simulationTime));

    // Schedule break of link B-C at t=5s
    Simulator::Schedule(Seconds(5.0), &PointToPointNetDevice::SetLinkStatus, DynamicCast<PointToPointNetDevice>(devicesBC.Get(0)), false);
    Simulator::Schedule(Seconds(5.0), &PointToPointNetDevice::SetLinkStatus, DynamicCast<PointToPointNetDevice>(devicesBC.Get(1)), false);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}