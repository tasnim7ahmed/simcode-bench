#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void SourceRxCallback(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
        NS_LOG_INFO("Source received packet of size " << packet->GetSize() << " bytes from " << addr.GetIpv4());
    }
}

void DestRxCallback(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
        NS_LOG_INFO("Destination received packet of size " << packet->GetSize() << " bytes from " << addr.GetIpv4());
    }
}

void SendPacket(Ptr<Socket> socket, Ipv4Address dst, uint16_t port, uint32_t size)
{
    Ptr<Packet> packet = Create<Packet>(size);
    socket->SendTo(packet, 0, InetSocketAddress(dst, port));
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4StaticRouting", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_ERROR);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_ERROR);
    LogComponentEnable("ns3::UdpSocketImpl", LOG_LEVEL_INFO);
    LogComponentEnable("ns3::Socket", LOG_LEVEL_INFO);

    // Nodes: src, r1, r2, dst
    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> src = nodes.Get(0);
    Ptr<Node> r1  = nodes.Get(1);
    Ptr<Node> r2  = nodes.Get(2);
    Ptr<Node> dst = nodes.Get(3);

    // Channels & assigning them
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Net 0: src <-> r1
    NodeContainer n0n1(src, r1);
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Net 1: r1 <-> dst
    NodeContainer n1n3(r1, dst);
    NetDeviceContainer d1d3 = p2p.Install(n1n3);

    // Net 2: src <-> r2
    NodeContainer n0n2(src, r2);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    // Net 3: r2 <-> dst
    NodeContainer n2n3(r2, dst);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // Internet stack
    InternetStackHelper internet;
    internet.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1); // src-r1

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = ipv4.Assign(d1d3); // r1-dst

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2); // src-r2

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3); // r2-dst

    // Enable static routing
    Ipv4StaticRoutingHelper staticRoutingHelper;

    // Source node routing: two routes to dst via r1 and r2 with different metrics
    Ptr<Ipv4StaticRouting> srcStaticRouting = staticRoutingHelper.GetStaticRouting(src->GetObject<Ipv4>());
    // Route 1: via r1
    srcStaticRouting->AddNetworkRouteTo("10.0.2.0", "255.255.255.0", i0i1.GetAddress(1), 1);
    // Route 2: via r2, higher metric
    srcStaticRouting->AddNetworkRouteTo("10.0.4.0", "255.255.255.0", i0i2.GetAddress(1), 10);

    // r1 routing: forward to dst
    Ptr<Ipv4StaticRouting> r1StaticRouting = staticRoutingHelper.GetStaticRouting(r1->GetObject<Ipv4>());
    r1StaticRouting->AddNetworkRouteTo("10.0.4.0", "255.255.255.0", i1i3.GetAddress(1), 1);
    r1StaticRouting->AddNetworkRouteTo("10.0.3.0", "255.255.255.0", i0i1.GetAddress(0), 1);
    r1StaticRouting->AddNetworkRouteTo("10.0.3.0", "255.255.255.0", i1i3.GetAddress(1), 1);

    // r2 routing: forward to dst
    Ptr<Ipv4StaticRouting> r2StaticRouting = staticRoutingHelper.GetStaticRouting(r2->GetObject<Ipv4>());
    r2StaticRouting->AddNetworkRouteTo("10.0.2.0", "255.255.255.0", i2i3.GetAddress(1), 1);
    r2StaticRouting->AddNetworkRouteTo("10.0.1.0", "255.255.255.0", i0i2.GetAddress(0), 1);

    // dst routing
    Ptr<Ipv4StaticRouting> dstStaticRouting = staticRoutingHelper.GetStaticRouting(dst->GetObject<Ipv4>());
    dstStaticRouting->AddNetworkRouteTo("10.0.1.0", "255.255.255.0", i1i3.GetAddress(0), 1);
    dstStaticRouting->AddNetworkRouteTo("10.0.3.0", "255.255.255.0", i2i3.GetAddress(0), 1);

    // Applications: use raw sockets for interface selection
    uint16_t port = 5000;
    Ptr<Socket> srcSocket1 = Socket::CreateSocket(src, UdpSocketFactory::GetTypeId());
    srcSocket1->SetAllowBroadcast(true);
    srcSocket1->Bind(InetSocketAddress(i0i1.GetAddress(0), 0)); // Bind to interface 0 (to r1)
    srcSocket1->SetRecvCallback(MakeCallback(&SourceRxCallback));

    Ptr<Socket> srcSocket2 = Socket::CreateSocket(src, UdpSocketFactory::GetTypeId());
    srcSocket2->SetAllowBroadcast(true);
    srcSocket2->Bind(InetSocketAddress(i0i2.GetAddress(0), 0)); // Bind to interface 2 (to r2)
    srcSocket2->SetRecvCallback(MakeCallback(&SourceRxCallback));

    Ptr<Socket> dstSocket = Socket::CreateSocket(dst, UdpSocketFactory::GetTypeId());
    InetSocketAddress dstLocal = InetSocketAddress(i1i3.GetAddress(1), port);
    dstSocket->Bind(dstLocal);
    dstSocket->SetRecvCallback(MakeCallback(&DestRxCallback));

    Ptr<Socket> dstSocket2 = Socket::CreateSocket(dst, UdpSocketFactory::GetTypeId());
    InetSocketAddress dstLocal2 = InetSocketAddress(i2i3.GetAddress(1), port);
    dstSocket2->Bind(dstLocal2);
    dstSocket2->SetRecvCallback(MakeCallback(&DestRxCallback));

    // Schedule two sends from different interfaces
    Simulator::Schedule(Seconds(1.0), &SendPacket, srcSocket1, i1i3.GetAddress(1), port, 1024);
    Simulator::Schedule(Seconds(2.0), &SendPacket, srcSocket2, i2i3.GetAddress(1), port, 2048);

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}