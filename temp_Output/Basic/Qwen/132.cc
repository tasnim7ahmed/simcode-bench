#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoNodeLoopProblem");

int main(int argc, char *argv[]) {
    bool verbose = true;
    uint32_t stopTime = 20;

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes A and B
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link between the two nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // Install Internet stack with Distance Vector routing
    InternetStackHelper stack;
    DvRoutingHelper dv;
    Ipv4ListRoutingHelper list;
    list.Add(dv, 0);  // priority 0 for DV
    stack.SetRoutingHelper(list);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create a remote server node to simulate destination
    NodeContainer remoteNode;
    remoteNode.Create(1);
    stack.Install(remoteNode);

    // Connect remote node to Node A via a new point-to-point link
    NodeContainer net1Nodes = NodeContainer(nodes.Get(0), remoteNode.Get(0));
    PointToPointHelper p2pRemote;
    p2pRemote.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRemote.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer net1Devices = p2pRemote.Install(net1Nodes);

    // Assign IP to remote network
    Ipv4AddressHelper remoteAddress;
    remoteAddress.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer remoteInterfaces = remoteAddress.Assign(net1Devices);

    // Set up default routes on remote node to reach the rest of the network
    Ipv4StaticRoutingHelper ipv4StaticRouting;
    Ptr<Ipv4> ipv4Remote = remoteNode.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> remoteRouting = ipv4StaticRouting.GetStaticRouting(ipv4Remote);
    remoteRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"),
                                    1);  // interface index of remote node's link to A

    // Start applications: Echo server on remote node, client on Node B
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(remoteNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(stopTime));

    UdpEchoClientHelper echoClient(remoteInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(5.0));
    clientApps.Stop(Seconds(stopTime));

    // Schedule link failure simulation: Disconnect remote node from Node A at t=10s
    Simulator::Schedule(Seconds(10.0), &Ipv4::SetDown, nodes.Get(0)->GetObject<Ipv4>(), net1Devices.Get(0));
    Simulator::Schedule(Seconds(10.0), &Ipv4::SetDown, remoteNode.Get(0)->GetObject<Ipv4>(), net1Devices.Get(1));

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}