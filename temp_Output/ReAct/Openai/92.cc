#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Declare nodes
    NodeContainer source, rtr1, rtr2, rtrDst, dest;
    source.Create(1);
    rtr1.Create(1);
    rtr2.Create(1);
    rtrDst.Create(1);
    dest.Create(1);

    // Create multi-interface node container for source
    NodeContainer srcRtr1(source.Get(0), rtr1.Get(0));
    NodeContainer srcRtr2(source.Get(0), rtr2.Get(0));
    NodeContainer rtr1RtrDst(rtr1.Get(0), rtrDst.Get(0));
    NodeContainer rtr2RtrDst(rtr2.Get(0), rtrDst.Get(0));
    NodeContainer rtrDstDest(rtrDst.Get(0), dest.Get(0));

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ndSrcRtr1 = p2p.Install(srcRtr1);
    NetDeviceContainer ndSrcRtr2 = p2p.Install(srcRtr2);
    NetDeviceContainer ndRtr1RtrDst = p2p.Install(rtr1RtrDst);
    NetDeviceContainer ndRtr2RtrDst = p2p.Install(rtr2RtrDst);
    NetDeviceContainer ndRtrDstDest = p2p.Install(rtrDstDest);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(source);
    stack.Install(rtr1);
    stack.Install(rtr2);
    stack.Install(rtrDst);
    stack.Install(dest);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifSrcRtr1 = ipv4.Assign(ndSrcRtr1);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifSrcRtr2 = ipv4.Assign(ndSrcRtr2);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifRtr1RtrDst = ipv4.Assign(ndRtr1RtrDst);

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifRtr2RtrDst = ipv4.Assign(ndRtr2RtrDst);

    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifRtrDstDest = ipv4.Assign(ndRtrDstDest);

    // Enable static routing
    Ipv4StaticRoutingHelper staticRouting;

    // Source Node: Two interfaces, add static routes for both paths
    Ptr<Ipv4> srcIpv4 = source.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> srcStatic = staticRouting.GetStaticRouting(srcIpv4);

    // Path 1: via Rtr1
    srcStatic->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), ifSrcRtr1.GetAddress(1), 1);

    // Path 2: via Rtr2
    srcStatic->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), ifSrcRtr2.GetAddress(1), 2);

    // Rtr1 static routing
    Ptr<Ipv4> r1Ipv4 = rtr1.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> r1Static = staticRouting.GetStaticRouting(r1Ipv4);
    r1Static->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), ifRtr1RtrDst.GetAddress(1), 2);
    r1Static->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), ifSrcRtr1.GetAddress(0), 1);
    r1Static->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), ifSrcRtr1.GetAddress(0), 1);

    // Rtr2 static routing
    Ptr<Ipv4> r2Ipv4 = rtr2.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> r2Static = staticRouting.GetStaticRouting(r2Ipv4);
    r2Static->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), ifRtr2RtrDst.GetAddress(1), 2);
    r2Static->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), ifSrcRtr2.GetAddress(0), 1);
    r2Static->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), ifSrcRtr2.GetAddress(0), 1);

    // RtrDst static routing
    Ptr<Ipv4> rDstIpv4 = rtrDst.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> rDstStatic = staticRouting.GetStaticRouting(rDstIpv4);
    rDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), ifRtr1RtrDst.GetAddress(0), 1);
    rDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), ifRtr2RtrDst.GetAddress(0), 2);
    rDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), ifRtrDstDest.GetAddress(1), 3);

    // Destination: Add default route to rtrDst
    Ptr<Ipv4> dstIpv4 = dest.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> dstStatic = staticRouting.GetStaticRouting(dstIpv4);
    dstStatic->SetDefaultRoute(ifRtrDstDest.GetAddress(0), 1);

    // TCP sink application at destination
    uint16_t port1 = 8080;
    uint16_t port2 = 8081;
    Address sinkLocalAddress1(InetSocketAddress(Ipv4Address::GetAny(), port1));
    Address sinkLocalAddress2(InetSocketAddress(Ipv4Address::GetAny(), port2));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkLocalAddress1);
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkLocalAddress2);

    ApplicationContainer sinkApps1 = sinkHelper1.Install(dest.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(10.0));

    ApplicationContainer sinkApps2 = sinkHelper2.Install(dest.Get(0));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(10.0));

    // TCP source: manually pick outgoing interface (bind to source IP)
    // Traffic via path 1 (Rtr1)
    Ptr<Socket> srcSocket1 = Socket::CreateSocket(source.Get(0), TcpSocketFactory::GetTypeId());
    srcSocket1->Bind(InetSocketAddress(ifSrcRtr1.GetAddress(0), 0));
    srcSocket1->Connect(InetSocketAddress(ifRtrDstDest.GetAddress(1), port1));

    // Traffic via path 2 (Rtr2)
    Ptr<Socket> srcSocket2 = Socket::CreateSocket(source.Get(0), TcpSocketFactory::GetTypeId());
    srcSocket2->Bind(InetSocketAddress(ifSrcRtr2.GetAddress(0), 0));
    srcSocket2->Connect(InetSocketAddress(ifRtrDstDest.GetAddress(1), port2));

    // Generate traffic for both flows
    Simulator::Schedule(Seconds(1.0), [srcSocket1](){
        Ptr<Packet> packet = Create<Packet>(1024);
        srcSocket1->Send(packet);
    });
    Simulator::Schedule(Seconds(3.0), [srcSocket2](){
        Ptr<Packet> packet = Create<Packet>(1024);
        srcSocket2->Send(packet);
    });

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}