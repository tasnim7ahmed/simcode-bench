#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("StarTopologySimulation", LOG_LEVEL_INFO);

    // Create nodes: 1 central and 2 peripheral
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> centralNode = nodes.Get(0);
    NodeContainer peripheralNodes(nodes.Get(1), nodes.Get(2));

    // Setup point-to-point links between central node and peripherals
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer link1 = p2p.Install(centralNode, nodes.Get(1));
    NetDeviceContainer link2 = p2p.Install(centralNode, nodes.Get(2));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(link1);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces2 = address.Assign(link2);

    // Enable logging for packet transmission
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Set up applications: UDP server on peripheral node 2, client on node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces2.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Animation output
    AnimationInterface anim("star-topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), -50, 50);
    anim.SetConstantPosition(nodes.Get(2), 50, 50);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}