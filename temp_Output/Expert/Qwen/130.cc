#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/nat-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NatSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer privateNodes;
    privateNodes.Create(2);

    NodeContainer natNode;
    natNode.Create(1);

    NodeContainer publicNode;
    publicNode.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer privateDevices;
    NetDeviceContainer publicDevice;

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer privateInterfaces;
    Ipv4InterfaceContainer natPrivateInterface;
    Ipv4InterfaceContainer natPublicInterface;
    Ipv4InterfaceContainer publicInterface;

    // Private network: 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    privateDevices = p2p.Install(privateNodes.Get(0), natNode.Get(0));
    privateInterfaces = address.Assign(privateDevices);

    privateDevices = p2p.Install(privateNodes.Get(1), natNode.Get(0));
    Ipv4InterfaceContainer privateInterfaces2 = address.Assign(privateDevices);

    // Public network: 203.0.113.0/24
    address.SetBase("203.0.113.0", "255.255.255.0");
    publicDevice = p2p.Install(natNode.Get(0), publicNode.Get(0));
    natPublicInterface = address.Assign(publicDevice);
    publicInterface = natPublicInterface;

    InternetStackHelper stack;
    stack.Install(privateNodes);
    stack.Install(natNode);
    stack.Install(publicNode);

    // NAT configuration
    NatHelper nat;
    nat.SetNatIpInterface(natPublicInterface.GetAddress(0)); // Public IP on NAT
    nat.Assign(privateInterfaces); // First private interface
    nat.Assign(privateInterfaces2); // Second private interface

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Server on public node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(publicNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Clients on private nodes
    UdpEchoClientHelper echoClient(publicInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient.Install(privateNodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = echoClient.Install(privateNodes.Get(1));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}