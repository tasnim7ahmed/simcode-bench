#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for NAT and applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("NatHelper", LOG_LEVEL_ALL);

    // Create containers for nodes
    NodeContainer privateNodes;
    privateNodes.Create(2);  // Two hosts in the private network

    NodeContainer natNode = NodeContainer::GetGlobal().Create(1);  // NAT router node

    NodeContainer publicNode;
    publicNode.Create(1);  // Public server node

    // Point-to-point helpers
    PointToPointHelper p2pPrivate;
    p2pPrivate.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pPrivate.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pPublic;
    p2pPublic.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pPublic.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(privateNodes);
    stack.Install(natNode);
    stack.Install(publicNode);

    // Assign IP addresses to private network links
    Ipv4AddressHelper privateIp;
    privateIp.SetBase("192.168.1.0", "255.255.255.0");
    NetDeviceContainer privateDevices = p2pPrivate.Install(privateNodes.Get(0), natNode.Get(0));
    Ipv4InterfaceContainer privateInterfaces = privateIp.Assign(privateDevices);
    privateIp.NewNetwork();
    NetDeviceContainer privateDevices2 = p2pPrivate.Install(privateNodes.Get(1), natNode.Get(0));
    Ipv4InterfaceContainer privateInterfaces2 = privateIp.Assign(privateDevices2);

    // Assign IP addresses to public network link
    Ipv4AddressHelper publicIp;
    publicIp.SetBase("203.0.113.0", "255.255.255.0");
    NetDeviceContainer publicDevices = p2pPublic.Install(natNode.Get(0), publicNode.Get(0));
    Ipv4InterfaceContainer publicInterfaces = publicIp.Assign(publicDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure NAT on the NAT router
    NatHelper nat;
    nat.SetNatIpAddress(publicInterfaces.GetAddress(0));  // Public IP of NAT
    nat.Assign(privateInterfaces.GetAddress(0), 1024);     // Private IP and start port for first host
    nat.Assign(privateInterfaces2.GetAddress(0), 2048);    // Private IP and start port for second host

    // Set up UDP Echo Server on public node (external server)
    uint16_t serverPort = 9;
    UdpEchoServerHelper echoServer(serverPort);
    ApplicationContainer serverApp = echoServer.Install(publicNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on private hosts
    UdpEchoClientHelper echoClient1(publicInterfaces.GetAddress(1), serverPort);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient1.Install(privateNodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(publicInterfaces.GetAddress(1), serverPort);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp2 = echoClient2.Install(privateNodes.Get(1));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}