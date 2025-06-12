#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set TCP variants
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::TcpSocketBase::CongestionControlType", EnumValue(2)); // Cubic

    // Create nodes for dumbbell topology
    NodeContainer leftNodes;
    leftNodes.Create(2);

    NodeContainer rightNode;
    rightNode.Create(1);

    NodeContainer bottleneck;
    bottleneck.Add(leftNodes.Get(0));
    bottleneck.Add(rightNode.Get(0));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Setup point-to-point links
    PointToPointHelper p2pLeft;
    p2pLeft.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pLeft.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("2Mbps"));  // Bottleneck link
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    // Left to bottleneck
    NetDeviceContainer devLeftRouter = p2pLeft.Install(leftNodes.Get(0), rightNode.Get(0));
    // Right node
    NetDeviceContainer devRight = p2pLeft.Install(leftNodes.Get(1), rightNode.Get(0));

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces = ip.Assign(devLeftRouter);
    ip.NewNetwork();
    Ipv4InterfaceContainer rightInterfaces = ip.Assign(devRight);

    // Install applications
    uint16_t port = 5000;

    // First flow: from left node 0 to right node
    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(1), port));
    source1.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSource1 = source1.Install(leftNodes.Get(0));
    appSource1.Start(Seconds(1.0));
    appSource1.Stop(Seconds(10.0));

    PacketSinkHelper sink1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appSink1 = sink1.Install(leftNodes.Get(1));
    appSink1.Start(Seconds(1.0));
    appSink1.Stop(Seconds(10.0));

    // Second flow: from left node 1 to right node
    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(1), port + 1));
    source2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSource2 = source2.Install(leftNodes.Get(1));
    appSource2.Start(Seconds(1.0));
    appSource2.Stop(Seconds(10.0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer appSink2 = sink2.Install(leftNodes.Get(1));
    appSink2.Start(Seconds(1.0));
    appSink2.Stop(Seconds(10.0));

    // Enable NetAnim
    AnimationInterface anim("congestion-control.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RoutingTables();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}