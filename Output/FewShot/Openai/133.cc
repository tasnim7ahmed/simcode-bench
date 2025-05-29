#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable DV routing logging for demonstration
    LogComponentEnable("DvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point link helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Setup A-B link (nodes 0-1)
    NodeContainer ab = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer abDevices = p2p.Install(ab);

    // Setup B-C link (nodes 1-2)
    NodeContainer bc = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer bcDevices = p2p.Install(bc);

    // Install Internet stack with Distance Vector (DV) routing and Split Horizon
    InternetStackHelper stack;
    DvRoutingHelper dvRouting;
    dvRouting.Set("SplitHorizon", BooleanValue(true));
    stack.SetRoutingHelper(dvRouting);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper abAddr;
    abAddr.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIfaces = abAddr.Assign(abDevices);

    Ipv4AddressHelper bcAddr;
    bcAddr.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIfaces = bcAddr.Assign(bcDevices);

    // Set up a UDP server on node C (node 2)
    uint16_t port = 9000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(20.0));

    // Set up a UDP client on node A (node 0) targeting node C
    UdpEchoClientHelper echoClient(bcIfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(20.0));

    // Enable routing table logging to demonstrate Split Horizon
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Simulator::Schedule(Seconds(4.0), [] {
        for (uint32_t i = 0; i < 3; ++i) {
            Ptr<Node> node = NodeList::GetNode(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            std::cout << "Routing table at time 4s for Node " << i << ":\n";
            ipv4->GetRoutingProtocol()->Print(std::cout);
            std::cout << std::endl;
        }
    });

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}