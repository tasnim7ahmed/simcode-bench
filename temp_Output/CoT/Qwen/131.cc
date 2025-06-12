#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinitySimulation");

int main(int argc, char *argv[]) {
    bool logging = true;
    uint32_t simulationTime = 50;
    std::string pointToPointDataRate = "1Mbps";
    std::string pointToPointDelay = "10ms";

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.Parse(argc, argv);

    if (logging) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("DvRoutingProtocol", LOG_LEVEL_ALL);
    }

    NodeContainer nodesA, nodesB, nodesC;
    Ptr<Node> nodeA = CreateObject<Node>();
    Ptr<Node> nodeB = CreateObject<Node>();
    Ptr<Node> nodeC = CreateObject<Node>();

    NodeContainer allNodes(nodeA, nodeB);
    allNodes.Add(nodeC);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(pointToPointDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(pointToPointDelay));

    NetDeviceContainer devicesAB = p2p.Install(nodeA, nodeB);
    NetDeviceContainer devicesBC = p2p.Install(nodeB, nodeC);
    NetDeviceContainer devicesCA = p2p.Install(nodeC, nodeA);

    InternetStackHelper stack;
    DvRoutingHelper dvRouting;
    stack.SetRoutingHelper(dvRouting);
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesCA = address.Assign(devicesCA);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Schedule(Seconds(5.0), &Ipv4InterfaceContainer::SetDown, &interfacesBC);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}