#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceStaticRoutingExample");

int main(int argc, char *argv[]) {
    // Enable logging for TCP
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6); // 0:Src, 1:Rtr1, 2:Rtr2, 3:RtrDst, 4:Dst

    Ptr<Node> src = nodes.Get(0);
    Ptr<Node> rtr1 = nodes.Get(1);
    Ptr<Node> rtr2 = nodes.Get(2);
    Ptr<Node> rtrDst = nodes.Get(3);
    Ptr<Node> dst = nodes.Get(4);

    // Setup point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Src <-> Rtr1
    NetDeviceContainer dev01 = p2p.Install(src, rtr1);

    // Src <-> Rtr2
    NetDeviceContainer dev02 = p2p.Install(src, rtr2);

    // Rtr1 <-> RtrDst
    NetDeviceContainer dev13 = p2p.Install(rtr1, rtrDst);

    // Rtr2 <-> RtrDst
    NetDeviceContainer dev23 = p2p.Install(rtr2, rtrDst);

    // RtrDst <-> Dst
    NetDeviceContainer dev34 = p2p.Install(rtrDst, dst);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(NodeContainer(src, rtr1, rtr2, rtrDst, dst));

    // Assign IP addresses (unique subnet for each link)
    Ipv4AddressHelper address;

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign(dev01);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if02 = address.Assign(dev02);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if13 = address.Assign(dev13);

    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = address.Assign(dev23);

    address.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer if34 = address.Assign(dev34);

    // Enable static routing on routers and host
    Ipv4StaticRoutingHelper staticRouting;

    // Source has two interfaces: 0 (to rtr1), 1 (to rtr2)
    Ptr<Ipv4> ipv4Src = src->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> srcStatic = staticRouting.GetStaticRouting(ipv4Src);

    // Rtr1
    Ptr<Ipv4> ipv4Rtr1 = rtr1->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> rtr1Static = staticRouting.GetStaticRouting(ipv4Rtr1);

    // Rtr2
    Ptr<Ipv4> ipv4Rtr2 = rtr2->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> rtr2Static = staticRouting.GetStaticRouting(ipv4Rtr2);

    // RtrDst
    Ptr<Ipv4> ipv4RtrDst = rtrDst->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> rtrDstStatic = staticRouting.GetStaticRouting(ipv4RtrDst);

    // Dst
    Ptr<Ipv4> ipv4Dst = dst->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> dstStatic = staticRouting.GetStaticRouting(ipv4Dst);

    // Configure static routing:
    // Let TCP flows use two explicit paths (via Rtr1 or via Rtr2)
    // Path 1: Src(if01) -> Rtr1 -> RtrDst -> Dst
    // Path 2: Src(if02) -> Rtr2 -> RtrDst -> Dst

    // On Rtr1: route to 10.0.5.0/24 via RtrDst
    rtr1Static->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), if13.GetAddress(1), 1);

    // On Rtr2: route to 10.0.5.0/24 via RtrDst
    rtr2Static->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), if23.GetAddress(1), 1);

    // On RtrDst: route to 10.0.1.0/24 via Rtr1; 10.0.2.0/24 via Rtr2
    rtrDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), if13.GetAddress(0), 1);
    rtrDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), if23.GetAddress(0), 2);

    // Default route on routers to reach everywhere else via the peer interfaces on upstream
    rtr1Static->SetDefaultRoute(if01.GetAddress(0), 1);
    rtr2Static->SetDefaultRoute(if02.GetAddress(0), 1);
    rtrDstStatic->SetDefaultRoute(if34.GetAddress(1), 3);
    dstStatic->SetDefaultRoute(if34.GetAddress(0), 1);

    // Static routes on Source: send to 10.0.5.2 (Dst)
    // We will create two routes: via Rtr1 (if01), metric 1; via Rtr2 (if02), metric 10
    srcStatic->AddHostRouteTo(if34.GetAddress(1), if01.GetAddress(1), 1); // via Rtr1
    srcStatic->AddHostRouteTo(if34.GetAddress(1), if02.GetAddress(1), 2); // via Rtr2

    // Applications:
    uint16_t port1 = 5001;
    uint16_t port2 = 5002;

    // Install PacketSink on dst for each socket
    Address sinkLocalAddress1(InetSocketAddress(Ipv4Address::GetAny(), port1));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkLocalAddress1);
    ApplicationContainer sinkApp1 = sinkHelper1.Install(dst);
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(20.0));

    Address sinkLocalAddress2(InetSocketAddress(Ipv4Address::GetAny(), port2));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkLocalAddress2);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(dst);
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(20.0));

    // Create sockets on source for both paths
    Ptr<Socket> srcSocket1 = Socket::CreateSocket(src, TcpSocketFactory::GetTypeId());
    Ptr<Socket> srcSocket2 = Socket::CreateSocket(src, TcpSocketFactory::GetTypeId());

    // Set output interface for each socket explicitly using Socket::BindToNetDevice
    Ptr<NetDevice> srcDev1 = dev01.Get(0); // to Rtr1
    Ptr<NetDevice> srcDev2 = dev02.Get(0); // to Rtr2

    srcSocket1->BindToNetDevice(srcDev1);
    srcSocket2->BindToNetDevice(srcDev2);

    // BulkSend for path 1 (via Rtr1)
    BulkSendHelper bulkSend1("ns3::TcpSocketFactory", InetSocketAddress(if34.GetAddress(1), port1));
    bulkSend1.SetAttribute("MaxBytes", UintegerValue(1000000));
    bulkSend1.SetAttribute("SendSize", UintegerValue(1024));
    bulkSend1.SetAttribute("Socket", PointerValue(srcSocket1));
    ApplicationContainer sourceApp1 = bulkSend1.Install(src);
    sourceApp1.Start(Seconds(1.0));
    sourceApp1.Stop(Seconds(10.0));

    // BulkSend for path 2 (via Rtr2)
    BulkSendHelper bulkSend2("ns3::TcpSocketFactory", InetSocketAddress(if34.GetAddress(1), port2));
    bulkSend2.SetAttribute("MaxBytes", UintegerValue(1000000));
    bulkSend2.SetAttribute("SendSize", UintegerValue(1024));
    bulkSend2.SetAttribute("Socket", PointerValue(srcSocket2));
    ApplicationContainer sourceApp2 = bulkSend2.Install(src);
    sourceApp2.Start(Seconds(11.0));
    sourceApp2.Stop(Seconds(19.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("multi-interface-static-routing", true);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}