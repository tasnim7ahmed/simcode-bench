#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer privateNodes, publicNodes, natRouterNode;
    privateNodes.Create(2);
    publicNodes.Create(1);
    natRouterNode.Create(1);

    // Combine all nodes
    NodeContainer allNodes;
    allNodes.Add(privateNodes);
    allNodes.Add(publicNodes);
    allNodes.Add(natRouterNode);

    // Point-to-point helper for private network
    PointToPointHelper privateP2P;
    privateP2P.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    privateP2P.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect private nodes to NAT router
    NetDeviceContainer privateDevices1 = privateP2P.Install(privateNodes.Get(0), natRouterNode.Get(0));
    NetDeviceContainer privateDevices2 = privateP2P.Install(privateNodes.Get(1), natRouterNode.Get(0));

    // Point-to-point helper for public network
    PointToPointHelper publicP2P;
    publicP2P.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    publicP2P.SetChannelAttribute("Delay", StringValue("1ms"));

    // Connect NAT router to public server
    NetDeviceContainer publicDevices = publicP2P.Install(natRouterNode.Get(0), publicNodes.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses for private network
    Ipv4AddressHelper privateAddress;
    privateAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer privateInterfaces1 = privateAddress.Assign(privateDevices1);
    Ipv4InterfaceContainer privateInterfaces2 = privateAddress.Assign(privateDevices2);

    // Assign IP addresses for public network
    Ipv4AddressHelper publicAddress;
    publicAddress.SetBase("20.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer publicInterfaces = publicAddress.Assign(publicDevices);

    // Configure NAT router
    Ptr<Node> natRouter = natRouterNode.Get(0);
    Ptr<NatRouter> nat = CreateObject<NatRouter>();
    natRouter->AddApplication(nat);
    nat->SetInside(privateInterfaces1.GetAddress(1));
    nat->SetOutside(publicInterfaces.GetAddress(0));
    nat->SetIpv4(natRouter->GetObject<Ipv4>());

    // Set up UDP echo server on public node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(publicNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP echo clients on private nodes
    UdpEchoClientHelper echoClient1(publicInterfaces.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    UdpEchoClientHelper echoClient2(publicInterfaces.GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));


    ApplicationContainer clientApp1 = echoClient1.Install(privateNodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    ApplicationContainer clientApp2 = echoClient2.Install(privateNodes.Get(1));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(10.0));

    // Add static route for public network
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}