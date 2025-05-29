#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // Enable logging for TCP congestion algorithms
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create 3 nodes: n0 (left), n1 (router), n2 (right)
    NodeContainer leftNode;
    leftNode.Create(1);

    NodeContainer rightNode;
    rightNode.Create(1);

    NodeContainer routerNode;
    routerNode.Create(1);

    // Create point-to-point links:
    // n0--(left link)--router--(right link)--n2

    // Left link (non-bottleneck)
    PointToPointHelper p2pLeft;
    p2pLeft.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    p2pLeft.SetChannelAttribute("Delay", StringValue("2ms"));

    // Right link (bottleneck)
    PointToPointHelper p2pRight;
    p2pRight.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRight.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install devices
    NetDeviceContainer devLeft = p2pLeft.Install(leftNode.Get(0), routerNode.Get(0));
    NetDeviceContainer devRight = p2pRight.Install(routerNode.Get(0), rightNode.Get(0));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(leftNode);
    stack.Install(routerNode);
    stack.Install(rightNode);

    // Assign IP addresses
    Ipv4AddressHelper addrLeft, addrRight;
    addrLeft.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceLeft = addrLeft.Assign(devLeft);

    addrRight.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceRight = addrRight.Assign(devRight);

    // Setup routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Two TCP flows from n0 to n2, different TCP variants (Reno and Cubic)
    // Create socket factory with different Tcp Congestion Control algorithms

    // First Flow: TCP Reno
    uint16_t sinkPort1 = 8080;
    Address sinkAddress1(InetSocketAddress(ifaceRight.GetAddress(1), sinkPort1));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort1));
    ApplicationContainer sinkApp1 = sinkHelper1.Install(rightNode.Get(0));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(20.0));

    // Second Flow: TCP Cubic
    uint16_t sinkPort2 = 8081;
    Address sinkAddress2(InetSocketAddress(ifaceRight.GetAddress(1), sinkPort2));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort2));
    ApplicationContainer sinkApp2 = sinkHelper2.Install(rightNode.Get(0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(20.0));

    // BulkSend App 1: TCP Reno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpReno")));
    BulkSendHelper source1("ns3::TcpSocketFactory", sinkAddress1);
    source1.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp1 = source1.Install(leftNode.Get(0));
    sourceApp1.Start(Seconds(1.0));
    sourceApp1.Stop(Seconds(15.0));

    // BulkSend App 2: TCP Cubic (use a new source node on the same side to simulate two flows)
    // For two flows sharing a bottleneck, we can reuse the same node (n0)
    // But different sockets required for different variants, so create two sockets
    Ptr<Node> n0 = leftNode.Get(0);

    // Create explicit socket with TCP Cubic
    Ptr<Socket> tcpSocket2 = Socket::CreateSocket(n0, TcpSocketFactory::GetTypeId());
    tcpSocket2->SetAttribute("CongestionControlAlgorithm", StringValue("TcpCubic"));

    BulkSendHelper source2("ns3::TcpSocketFactory", sinkAddress2);
    source2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp2 = source2.Install(leftNode.Get(0));
    sourceApp2.Start(Seconds(1.2));
    sourceApp2.Stop(Seconds(15.0));

    // NetAnim setup
    AnimationInterface anim("tcp-dumbbell.xml");
    anim.SetConstantPosition(leftNode.Get(0), 10.0, 25.0);
    anim.SetConstantPosition(routerNode.Get(0), 60.0, 25.0);
    anim.SetConstantPosition(rightNode.Get(0), 110.0, 25.0);

    // Mark packet flows
    anim.UpdateNodeDescription(leftNode.Get(0), "Sender");
    anim.UpdateNodeDescription(routerNode.Get(0), "Router");
    anim.UpdateNodeDescription(rightNode.Get(0), "Receiver");

    anim.UpdateNodeColor(leftNode.Get(0), 0, 255, 0);
    anim.UpdateNodeColor(routerNode.Get(0), 255, 255, 0);
    anim.UpdateNodeColor(rightNode.Get(0), 255, 0, 0);

    // Simulation start
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}