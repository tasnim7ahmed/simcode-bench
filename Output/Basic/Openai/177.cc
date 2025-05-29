#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpCongestionComparison");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer leftNodes;
    leftNodes.Create(2);

    NodeContainer rightNode;
    rightNode.Create(1);

    NodeContainer routerNode;
    routerNode.Create(1);

    // Point-to-point links
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("4Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("20ms"));

    // Connect left1 <-> router
    NetDeviceContainer devices_left1_router = accessLink.Install(leftNodes.Get(0), routerNode.Get(0));
    // Connect left2 <-> router
    NetDeviceContainer devices_left2_router = accessLink.Install(leftNodes.Get(1), routerNode.Get(0));
    // Connect router <-> right
    NetDeviceContainer devices_router_right = bottleneck.Install(routerNode.Get(0), rightNode.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(routerNode);
    stack.Install(rightNode);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_left1_router = ipv4.Assign(devices_left1_router);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_left2_router = ipv4.Assign(devices_left2_router);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_router_right = ipv4.Assign(devices_router_right);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up TCP variants for each sender
    TypeId tcpRenoTypeId = TypeId::LookupByName("ns3::TcpReno");
    TypeId tcpCubicTypeId = TypeId::LookupByName("ns3::TcpCubic");

    // TCP Flow 1: left1 -> right (TCP Reno)
    uint16_t port1 = 50000;
    Address sinkAddress1 (InetSocketAddress(if_router_right.GetAddress(1), port1));
    PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApp1 = sinkHelper1.Install(rightNode.Get(0));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(15.0));

    Ptr<Socket> tcpSocket1 = Socket::CreateSocket(leftNodes.Get(0), TcpSocketFactory::GetTypeId());
    tcpSocket1->SetAttribute("CongestionControlAlgorithm", StringValue("ns3::TcpReno"));

    BulkSendHelper source1("ns3::TcpSocketFactory", sinkAddress1);
    source1.SetAttribute("MaxBytes", UintegerValue(0));
    source1.SetAttribute("SendSize", UintegerValue(1448));
    source1.SetAttribute("SocketType", TypeIdValue(tcpRenoTypeId));
    ApplicationContainer sourceApp1 = source1.Install(leftNodes.Get(0));
    sourceApp1.Start(Seconds(1.0));
    sourceApp1.Stop(Seconds(15.0));

    // TCP Flow 2: left2 -> right (TCP Cubic)
    uint16_t port2 = 50001;
    Address sinkAddress2 (InetSocketAddress(if_router_right.GetAddress(1), port2));
    PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp2 = sinkHelper2.Install(rightNode.Get(0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(15.0));

    Ptr<Socket> tcpSocket2 = Socket::CreateSocket(leftNodes.Get(1), TcpSocketFactory::GetTypeId());
    tcpSocket2->SetAttribute("CongestionControlAlgorithm", StringValue("ns3::TcpCubic"));

    BulkSendHelper source2("ns3::TcpSocketFactory", sinkAddress2);
    source2.SetAttribute("MaxBytes", UintegerValue(0));
    source2.SetAttribute("SendSize", UintegerValue(1448));
    source2.SetAttribute("SocketType", TypeIdValue(tcpCubicTypeId));
    ApplicationContainer sourceApp2 = source2.Install(leftNodes.Get(1));
    sourceApp2.Start(Seconds(1.0));
    sourceApp2.Stop(Seconds(15.0));

    // Animation setup
    AnimationInterface anim("tcp-congestion-netanim.xml");
    anim.SetConstantPosition(leftNodes.Get(0), 10.0, 30.0);
    anim.SetConstantPosition(leftNodes.Get(1), 10.0, 10.0);
    anim.SetConstantPosition(routerNode.Get(0), 40.0, 20.0);
    anim.SetConstantPosition(rightNode.Get(0), 70.0, 20.0);

    // Highlight bottleneck device
    anim.UpdateNodeDescription(routerNode.Get(0), "Router/Bottleneck");
    anim.UpdateNodeColor(routerNode.Get(0), 255, 0, 0); // red for router

    // Run simulation
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}