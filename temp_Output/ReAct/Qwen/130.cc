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

    // Create nodes
    NodeContainer privateNodes;
    privateNodes.Create(2);
    Ptr<Node> natNode = CreateObject<Node>();
    Ptr<Node> publicServer = CreateObject<Node>();

    NodeContainer allNodes(privateNodes);
    allNodes.Add(natNode);
    allNodes.Add(publicServer);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Private network (192.168.1.0/24)
    NetDeviceContainer privateDevices[2];
    for (int i = 0; i < 2; ++i) {
        privateDevices[i] = p2p.Install(privateNodes.Get(i), natNode);
    }

    // Public network (10.0.0.0/24)
    NetDeviceContainer publicDevice = p2p.Install(natNode, publicServer);

    // Install Internet stacks
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Private network: 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer privateInterfaces[2];
    for (int i = 0; i < 2; ++i) {
        privateInterfaces[i] = address.Assign(privateDevices[i]);
        address.NewNetwork();
    }

    // Public network: 10.0.0.0/24
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer publicInterfaces = address.Assign(publicDevice);

    // Set up NAT on the NAT router node
    NatHelper natHelper;
    natHelper.SetNatIp("10.0.0.1"); // Public IP of NAT router
    natHelper.Install(natNode);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install applications
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(publicServer);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(publicInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps[2];
    for (int i = 0; i < 2; ++i) {
        clientApps[i] = echoClient.Install(privateNodes.Get(i));
        clientApps[i].Start(Seconds(2.0 + i));
        clientApps[i].Stop(Seconds(10.0));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}