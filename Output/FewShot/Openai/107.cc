#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include <fstream>

using namespace ns3;

void RxTraceCallback(std::string context, Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream rxTrace;
    static bool first = true;
    if (first)
    {
        rxTrace.open("fragmentation-ipv6-two-mtu.tr");
        first = false;
    }
    rxTrace << Simulator::Now().GetSeconds() << " " << context << " PacketReceived " << packet->GetSize() << " bytes" << std::endl;
}

void QueueTraceCallback(std::string context, Ptr<const QueueItem> item)
{
    static std::ofstream file;
    static bool first = true;
    if (first)
    {
        file.open("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
        first = false;
    }
    file << Simulator::Now().GetSeconds() << " " << context << " Enqueue " << item->GetPacket()->GetSize() << " bytes" << std::endl;
}

void DequeueTraceCallback(std::string context, Ptr<const QueueItem> item)
{
    static std::ofstream file;
    static bool first = true;
    if (first)
    {
        file.open("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
        first = false;
    }
    file << Simulator::Now().GetSeconds() << " " << context << " Dequeue " << item->GetPacket()->GetSize() << " bytes" << std::endl;
}

void DropTraceCallback(std::string context, Ptr<const QueueItem> item)
{
    static std::ofstream file;
    static bool first = true;
    if (first)
    {
        file.open("fragmentation-ipv6-two-mtu.tr", std::ios_base::app);
        first = false;
    }
    file << Simulator::Now().GetSeconds() << " " << context << " Drop " << item->GetPacket()->GetSize() << " bytes" << std::endl;
}

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer src, n0, r, n1, dst;
    src.Create(1);
    n0.Create(1);
    r.Create(1);
    n1.Create(1);
    dst.Create(1);

    NodeContainer net0(src.Get(0), n0.Get(0), r.Get(0));
    NodeContainer net1(r.Get(0), n1.Get(0), dst.Get(0));

    // CSMA helpers
    CsmaHelper csma0;
    csma0.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma0.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma0.SetDeviceAttribute("Mtu", UintegerValue(5000));

    CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma1.SetDeviceAttribute("Mtu", UintegerValue(1500));

    // Install devices
    NetDeviceContainer devNet0, devNet1;
    devNet0 = csma0.Install(net0);
    devNet1 = csma1.Install(net1);

    // Internet stack IPv6
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(src);
    internetv6.Install(n0);
    internetv6.Install(r);
    internetv6.Install(n1);
    internetv6.Install(dst);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6Addr0;
    ipv6Addr0.SetBase(Ipv6Address("2001:0:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaceNet0 = ipv6Addr0.Assign(devNet0);
    ifaceNet0.SetForwarding(0, true);
    ifaceNet0.SetForwarding(1, true);
    ifaceNet0.SetForwarding(2, true);
    ifaceNet0.SetDefaultRouteInAllNodes(2);

    Ipv6AddressHelper ipv6Addr1;
    ipv6Addr1.SetBase(Ipv6Address("2001:0:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaceNet1 = ipv6Addr1.Assign(devNet1);
    ifaceNet1.SetForwarding(0, true);
    ifaceNet1.SetForwarding(1, true);
    ifaceNet1.SetForwarding(2, true);
    ifaceNet1.SetDefaultRouteInAllNodes(0);

    // Enable static routing
    Ipv6StaticRoutingHelper staticRoutingHelper;

    Ptr<Ipv6> n0v6 = n0.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6> r_v6 = r.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6> n1v6 = n1.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6> srcv6 = src.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6> dstv6 = dst.Get(0)->GetObject<Ipv6>();

    Ptr<Ipv6StaticRouting> n0StaticRouting = staticRoutingHelper.GetStaticRouting(n0v6);
    n0StaticRouting->SetDefaultRoute(ifaceNet0.GetAddress(2, 1), 1);

    Ptr<Ipv6StaticRouting> rStaticRouting = staticRoutingHelper.GetStaticRouting(r_v6);
    rStaticRouting->AddNetworkRouteTo(Ipv6Address("2001:0:0:2::"), 64, ifaceNet1.GetAddress(0, 1), 2);
    rStaticRouting->AddNetworkRouteTo(Ipv6Address("2001:0:0:1::"), 64, ifaceNet0.GetAddress(2, 1), 1);

    Ptr<Ipv6StaticRouting> n1StaticRouting = staticRoutingHelper.GetStaticRouting(n1v6);
    n1StaticRouting->SetDefaultRoute(ifaceNet1.GetAddress(0, 1), 1);

    Ptr<Ipv6StaticRouting> srcStaticRouting = staticRoutingHelper.GetStaticRouting(srcv6);
    srcStaticRouting->SetDefaultRoute(ifaceNet0.GetAddress(1, 1), 1);

    Ptr<Ipv6StaticRouting> dstStaticRouting = staticRoutingHelper.GetStaticRouting(dstv6);
    dstStaticRouting->SetDefaultRoute(ifaceNet1.GetAddress(1, 1), 1);

    // UDP Echo Server on n1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(n1.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP Echo Client on n0, sends to n1
    Ipv6Address n1Addr = ifaceNet1.GetAddress(1, 1);
    UdpEchoClientHelper echoClient(n1Addr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(6));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(3000));
    ApplicationContainer clientApps = echoClient.Install(n0.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable routing
    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // Trace packet receptions at n1
    Config::Connect("/NodeList/" + std::to_string(n1.Get(0)->GetId()) +
                    "/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeCallback(&RxTraceCallback));

    // Trace queue events on both csma channels
    Ptr<NetDeviceQueueInterface> queueIntf0 = devNet0.Get(1)->GetObject<NetDeviceQueueInterface>();
    if (queueIntf0)
    {
        Ptr<QueueDisc> qDisc = DynamicCast<QueueDisc>(queueIntf0->GetTxQueue(0));
        if (qDisc)
        {
            qDisc->TraceConnect("Enqueue", "csma0", MakeCallback(&QueueTraceCallback));
            qDisc->TraceConnect("Dequeue", "csma0", MakeCallback(&DequeueTraceCallback));
            qDisc->TraceConnect("Drop", "csma0", MakeCallback(&DropTraceCallback));
        }
    }
    Ptr<NetDeviceQueueInterface> queueIntf1 = devNet1.Get(0)->GetObject<NetDeviceQueueInterface>();
    if (queueIntf1)
    {
        Ptr<QueueDisc> qDisc = DynamicCast<QueueDisc>(queueIntf1->GetTxQueue(0));
        if (qDisc)
        {
            qDisc->TraceConnect("Enqueue", "csma1", MakeCallback(&QueueTraceCallback));
            qDisc->TraceConnect("Dequeue", "csma1", MakeCallback(&DequeueTraceCallback));
            qDisc->TraceConnect("Drop", "csma1", MakeCallback(&DropTraceCallback));
        }
    }

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}