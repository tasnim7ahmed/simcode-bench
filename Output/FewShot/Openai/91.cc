#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceStaticRoutingExample");

// Packet receive callback for logging
void ReceivePacket(Ptr<Socket> socket, const Address &from)
{
    Ptr<Packet> packet;
    Address srcAddress;
    while ((packet = socket->RecvFrom(srcAddress)))
    {
        InetSocketAddress inetAddr = InetSocketAddress::ConvertFrom(srcAddress);
        NS_LOG_INFO("At " << Simulator::Now().GetSeconds()
                          << "s, Node " << socket->GetNode()->GetId()
                          << " received " << packet->GetSize()
                          << " bytes from " << inetAddr.GetIpv4()
                          << " port " << inetAddr.GetPort());
    }
}

// Function to send UDP packets, binds to different source interfaces
void ScheduleUdpSend(Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t dstPort, Ipv4Address bindAddr, uint32_t pktSize, uint32_t count, Time interval)
{
    if (count == 0)
    {
        socket->Close();
        return;
    }
    // Bind to the specified (source) local address
    InetSocketAddress local = InetSocketAddress(bindAddr, 0);
    socket->Bind(local);

    Ptr<Packet> packet = Create<Packet>(pktSize);
    InetSocketAddress remote = InetSocketAddress(dstAddr, dstPort);
    socket->SendTo(packet, 0, remote);

    NS_LOG_INFO("At " << Simulator::Now().GetSeconds()
                      << "s, Node " << socket->GetNode()->GetId()
                      << " sent " << pktSize << " bytes to " << dstAddr
                      << " using source " << bindAddr);

    // Unbind and re-bind for the next packet. Re-binding is done before the next send.
    Simulator::Schedule(interval, &ScheduleUdpSend, socket, dstAddr, dstPort, bindAddr, pktSize, count - 1, interval);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("MultiInterfaceStaticRoutingExample", LOG_LEVEL_INFO);

    // Create nodes: Source, R1, R2, Destination
    NodeContainer nodes;
    nodes.Create(4); // 0=Source, 1=R1, 2=R2, 3=Dest

    NodeContainer src = NodeContainer(nodes.Get(0));
    NodeContainer r1 = NodeContainer(nodes.Get(1));
    NodeContainer r2 = NodeContainer(nodes.Get(2));
    NodeContainer dst = NodeContainer(nodes.Get(3));

    // Point-to-point connections
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Topology:
    // src---if0---(if0)R1(if1)---if0---dst
    //     \-if1---(if0)R2(if1)---if1---dst

    // Links
    // src<->R1
    NetDeviceContainer srcR1 = p2p.Install(src.Get(0), r1.Get(0));
    // src<->R2 (second interface on src)
    NetDeviceContainer srcR2 = p2p.Install(src.Get(0), r2.Get(0));
    // R1<->dst
    NetDeviceContainer r1Dst = p2p.Install(r1.Get(0), dst.Get(0));
    // R2<->dst (second interface on dst)
    NetDeviceContainer r2Dst = p2p.Install(r2.Get(0), dst.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper address;

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_srcR1 = address.Assign(srcR1); // src - R1

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_srcR2 = address.Assign(srcR2); // src - R2

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_r1Dst = address.Assign(r1Dst); // R1 - dst

    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_r2Dst = address.Assign(r2Dst); // R2 - dst

    // Enable routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Manually set static routes with different metrics, so that both paths are valid but with differing metrics

    // SOURCE node: install static routes to dst via both routers, with different metrics
    Ptr<Ipv4StaticRouting> srcStatic = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(src.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());

    // First route: via R1 (metric 10)
    srcStatic->AddNetworkRouteTo(
        Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"),
        if_srcR1.GetAddress(1), // next-hop: R1 interface
        1, // src iface index for 10.0.1.0/24
        10);

    // Second route: via R2 (metric 20)
    srcStatic->AddNetworkRouteTo(
        Ipv4Address("10.0.4.0"), Ipv4Mask("255.255.255.0"),
        if_srcR2.GetAddress(1), // next-hop: R2 interface
        2, // src iface index for 10.0.2.0/24
        20);

    // R1: route to dst
    Ptr<Ipv4StaticRouting> r1Static = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(r1.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    r1Static->AddNetworkRouteTo(
        Ipv4Address("10.0.4.0"), Ipv4Mask("255.255.255.0"),
        if_r1Dst.GetAddress(1), // next-hop: dst
        1); // iface index

    // R2: route to dst
    Ptr<Ipv4StaticRouting> r2Static = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(r2.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    r2Static->AddNetworkRouteTo(
        Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"),
        if_r2Dst.GetAddress(1),
        1);

    // DST: route back to src
    Ptr<Ipv4StaticRouting> dstStatic = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(dst.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    dstStatic->AddNetworkRouteTo(
        Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"),
        if_r1Dst.GetAddress(0),
        1);

    dstStatic->AddNetworkRouteTo(
        Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"),
        if_r2Dst.GetAddress(0),
        2);

    // Set up UDP receiver at destination
    uint16_t udpPort = 4000;
    Ptr<Socket> dstSock = Socket::CreateSocket(dst.Get(0), UdpSocketFactory::GetTypeId());
    InetSocketAddress dstAddr = InetSocketAddress(Ipv4Address::GetAny(), udpPort);
    dstSock->Bind(dstAddr);
    dstSock->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Set up UDP receiver at source (to see ICMP errors or possible echoes)
    Ptr<Socket> srcSockRecv = Socket::CreateSocket(src.Get(0), UdpSocketFactory::GetTypeId());
    InetSocketAddress srcAnyAddr = InetSocketAddress(Ipv4Address::GetAny(), 9000);
    srcSockRecv->Bind(srcAnyAddr);
    srcSockRecv->SetRecvCallback(MakeCallback(&ReceivePacket));

    // Prepare sending sockets (one for each source interface, to bind explicitly)
    Ptr<Socket> srcSocket1 = Socket::CreateSocket(src.Get(0), UdpSocketFactory::GetTypeId());
    Ptr<Socket> srcSocket2 = Socket::CreateSocket(src.Get(0), UdpSocketFactory::GetTypeId());

    // Send some packets from Source to Destination, using both source IPs by binding each socket to one local interface address
    uint32_t pktSize = 512;
    uint32_t count = 4;
    Time interval = Seconds(1.0);

    // Schedule traffic from src (interface 0) to dst
    Simulator::Schedule(Seconds(2.0),
                        &ScheduleUdpSend,
                        srcSocket1,
                        if_r1Dst.GetAddress(1), // destination IP (dst side of R1-DST link)
                        udpPort,
                        if_srcR1.GetAddress(0), // source interface IP (src side of SRC-R1 link)
                        pktSize,
                        count,
                        interval);

    // Schedule traffic from src (interface 1) to dst
    Simulator::Schedule(Seconds(3.0),
                        &ScheduleUdpSend,
                        srcSocket2,
                        if_r2Dst.GetAddress(1), // destination IP (dst side of R2-DST link)
                        udpPort,
                        if_srcR2.GetAddress(0), // source interface IP (src side of SRC-R2 link)
                        pktSize,
                        count,
                        interval);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}