#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControlComparison");

int main(int argc, char *argv[])
{
    double simTime = 10.0; // seconds
    std::string bottleneckBandwidth = "2Mbps";
    std::string bottleneckDelay = "10ms";
    std::string leafBandwidth = "10Mbps";
    std::string leafDelay = "1ms";

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: n0----n1----n2 (dumbbell topology)
    NodeContainer leftNode, rightNode, routerNode;
    leftNode.Create(1);
    rightNode.Create(1);
    routerNode.Create(1);

    // Containers for connections
    NodeContainer n0n1(leftNode.Get(0), routerNode.Get(0));
    NodeContainer n2n1(rightNode.Get(0), routerNode.Get(0));

    // Point-to-Point links
    PointToPointHelper p2pLeaf, p2pBottleneck;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue(leafBandwidth));
    p2pLeaf.SetChannelAttribute("Delay", StringValue(leafDelay));
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    // Install devices
    NetDeviceContainer dev_n0n1 = p2pLeaf.Install(n0n1);
    NetDeviceContainer dev_n2n1 = p2pLeaf.Install(n2n1);

    // To create dumbbell, connect router to both left and right
    NetDeviceContainer dev_n1n0, dev_n1n2;
    dev_n1n0.Add(dev_n0n1.Get(1)); // router side
    dev_n1n2.Add(dev_n2n1.Get(1)); // router side

    // Now create the bottleneck link (router node to itself, 2 interfaces)
    // We use a bridge: not required here; just join the two right ports virtually
    // Actually, dumbbell is two sources via router to one sink, but here we have two interfaces.

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(leftNode);
    stack.Install(routerNode);
    stack.Install(rightNode);

    // Assign IPv4 Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0n1 = address.Assign(dev_n0n1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n2n1 = address.Assign(dev_n2n1);

    // Create the bottleneck link between routerNode and itself to model the bottleneck
    // Actually, for dumbbell topology, let's create as two sources to one sink, all pass through router. To make congestion, we must create two sources each via own leaf link to router, and router via point-to-point bottleneck to sink.

    // New plan: Left (n0, n2), router (n1), right (n3)
    NodeContainer leftNodes, rightNodes, router;
    leftNodes.Create(2); // n0, n2 (sources)
    rightNodes.Create(1); // n3 (sink)
    router.Create(1); // n1 (router)

    // Left leaf links
    NodeContainer n0n1(leftNodes.Get(0), router.Get(0));
    NodeContainer n2n1(leftNodes.Get(1), router.Get(0));
    // Bottleneck
    NodeContainer n1n3(router.Get(0), rightNodes.Get(0));

    NetDeviceContainer dev_n0n1 = p2pLeaf.Install(n0n1);
    NetDeviceContainer dev_n2n1 = p2pLeaf.Install(n2n1);
    NetDeviceContainer dev_n1n3 = p2pBottleneck.Install(n1n3);

    // Install stack
    stack.Install(leftNodes);
    stack.Install(router);
    stack.Install(rightNodes);

    // Assign addresses
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0n1 = address.Assign(dev_n0n1);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n2n1 = address.Assign(dev_n2n1);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1n3 = address.Assign(dev_n1n3);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic Control (optional: drop-tail FIFO queue, can have RED etc)
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    tch.Install(dev_n1n3);

    // Applications: two TCP flows, one with Reno, one with Cubic
    uint16_t port1 = 50000;
    uint16_t port2 = 50001;

    // BulkSend on source, PacketSink on sink

    // Sink App for flow 1
    Address sinkAddress1(InetSocketAddress(i_n1n3.GetAddress(1), port1));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApps1 = sinkHelper1.Install(rightNodes.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(simTime+1));

    // Sink App for flow 2
    Address sinkAddress2(InetSocketAddress(i_n1n3.GetAddress(1), port2));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApps2 = sinkHelper2.Install(rightNodes.Get(0));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(simTime+1));

    // Source 1 (Reno)
    TypeId tidReno = TypeId::LookupByName ("ns3::TcpReno");
    Ptr<Socket> ns3TcpSocket1 = Socket::CreateSocket(leftNodes.Get(0), TcpSocketFactory::GetTypeId());
    ns3TcpSocket1->SetAttribute("CongestionControlAlgorithm", tidReno);

    BulkSendHelper source1 ("ns3::TcpSocketFactory", sinkAddress1);
    source1.SetAttribute("MaxBytes", UintegerValue(0));
    source1.SetAttribute("SendSize", UintegerValue(1448));
    ApplicationContainer sourceApps1 = source1.Install(leftNodes.Get(0));
    sourceApps1.Start(Seconds(0.5));
    sourceApps1.Stop(Seconds(simTime));

    // Source 2 (Cubic)
    TypeId tidCubic = TypeId::LookupByName ("ns3::TcpCubic");
    Ptr<Socket> ns3TcpSocket2 = Socket::CreateSocket(leftNodes.Get(1), TcpSocketFactory::GetTypeId());
    ns3TcpSocket2->SetAttribute("CongestionControlAlgorithm", tidCubic);

    BulkSendHelper source2 ("ns3::TcpSocketFactory", sinkAddress2);
    source2.SetAttribute("MaxBytes", UintegerValue(0));
    source2.SetAttribute("SendSize", UintegerValue(1448));
    ApplicationContainer sourceApps2 = source2.Install(leftNodes.Get(1));
    sourceApps2.Start(Seconds(0.5));
    sourceApps2.Stop(Seconds(simTime));

    // Set attribute for different TCP variants at node basis
    Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpReno")));
    Config::Set("/NodeList/1/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));

    // Enable tracing
    AsciiTraceHelper ascii;
    p2pBottleneck.EnableAsciiAll(ascii.CreateFileStream("bottleneck.tr"));
    p2pBottleneck.EnablePcapAll("bottleneck", true);

    // NetAnim setup
    AnimationInterface anim("tcp-congestion-control.xml");
    anim.SetConstantPosition(leftNodes.Get(0), 10.0, 20.0);  // source 1
    anim.SetConstantPosition(leftNodes.Get(1), 10.0, 40.0);  // source 2
    anim.SetConstantPosition(router.Get(0), 50.0, 30.0);     // router
    anim.SetConstantPosition(rightNodes.Get(0), 90.0, 30.0); // sink

    anim.UpdateNodeDescription(leftNodes.Get(0), "Source1-Reno");
    anim.UpdateNodeDescription(leftNodes.Get(1), "Source2-Cubic");
    anim.UpdateNodeDescription(router.Get(0), "Router");
    anim.UpdateNodeDescription(rightNodes.Get(0), "Sink");
    anim.UpdateNodeColor(leftNodes.Get(0), 255, 0, 0);   // red
    anim.UpdateNodeColor(leftNodes.Get(1), 0, 0, 255);   // blue
    anim.UpdateNodeColor(router.Get(0), 0, 255, 0);      // green
    anim.UpdateNodeColor(rightNodes.Get(0), 0, 0, 0);    // black

    Simulator::Stop(Seconds(simTime+1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}