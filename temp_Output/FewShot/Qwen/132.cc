#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging for Distance Vector routing
    LogComponentEnable("DvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes A and B
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link between node 0 (A) and node 1 (B)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack with DV routing
    InternetStackHelper stack;
    DvRoutingHelper dv;
    stack.SetRoutingHelper(dv); // has to be set before installing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Assign a common destination network reachable via both nodes
    Ipv4AddressHelper destAddress;
    destAddress.SetBase("192.168.1.0", "255.255.255.0");

    // Initially route destination through the link
    Ipv4StaticRoutingHelper ipv4StaticRouting;
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();

    // Assume both nodes can reach 192.168.1.0/24 through each other initially
    // This is an artificial setup to allow initial convergence
    Ipv4RoutingTableEntry routeToDest = Ipv4RoutingTableEntry::CreateNetworkRouteTo(destAddress.NewNetwork(), interfaces.GetAddress(1), 1);
    ipv4A->GetRoutingProtocol()->NotifyAddRoute(routeToDest);
    routeToDest = Ipv4RoutingTableEntry::CreateNetworkRouteTo(destAddress.NewNetwork(), interfaces.GetAddress(0), 1);
    ipv4B->GetRoutingProtocol()->NotifyAddRoute(routeToDest);

    // Now simulate traffic from another source to trigger routing updates
    NodeContainer externalNode;
    externalNode.Create(1);
    NetDeviceContainer extDevice = p2p.Install(nodes.Get(0), externalNode.Get(0));
    address.SetBase("10.2.2.0", "255.255.255.0");
    address.Assign(extDevice);

    // Install UDP Echo Server on external node as a destination
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(externalNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Client app on node B trying to reach the server
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(nodes.Get(1));
    clientApp.Start(Seconds(4.0));
    clientApp.Stop(Seconds(20.0));

    // Simulate failure by removing route at time 7 seconds
    Simulator::Schedule(Seconds(7.0), [ipv4A, destAddress]() {
        Ipv4RoutingTableEntry routeToRemove = Ipv4RoutingTableEntry::CreateNetworkRouteTo(destAddress.NewNetwork(), Ipv4Address::GetZero(), 0);
        ipv4A->GetRoutingProtocol()->NotifyRemoveRoute(routeToRemove);
    });

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}