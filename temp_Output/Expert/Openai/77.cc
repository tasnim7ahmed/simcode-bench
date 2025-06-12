#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ripng-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(6); // 0: SRC, 1: A, 2: B, 3: C, 4: D, 5: DST

    // Naming for easier reference
    Ptr<Node> SRC = nodes.Get(0);
    Ptr<Node> A   = nodes.Get(1);
    Ptr<Node> B   = nodes.Get(2);
    Ptr<Node> C   = nodes.Get(3);
    Ptr<Node> D   = nodes.Get(4);
    Ptr<Node> DST = nodes.Get(5);

    // NodeContainers for each link
    NodeContainer n_SRC_A (SRC, A);
    NodeContainer n_A_B   (A, B);
    NodeContainer n_A_C   (A, C);
    NodeContainer n_B_C   (B, C);
    NodeContainer n_B_D   (B, D);
    NodeContainer n_C_D   (C, D);
    NodeContainer n_D_DST (D, DST);

    // Helper for all links except C-D
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Helper for C-D link with higher metric
    PointToPointHelper p2pCost10;
    p2pCost10.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2pCost10.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Install devices
    NetDeviceContainer d_SRC_A = p2p.Install(n_SRC_A);
    NetDeviceContainer d_A_B   = p2p.Install(n_A_B);
    NetDeviceContainer d_A_C   = p2p.Install(n_A_C);
    NetDeviceContainer d_B_C   = p2p.Install(n_B_C);
    NetDeviceContainer d_B_D   = p2p.Install(n_B_D);
    NetDeviceContainer d_C_D   = p2pCost10.Install(n_C_D);
    NetDeviceContainer d_D_DST = p2p.Install(n_D_DST);

    // Install the Internet stack with RIPng
    RipNgHelper ripNg;
    ripNg.ExcludeInterface(SRC, 1); // Don't run RIPng on SRC
    ripNg.ExcludeInterface(DST, 1); // Don't run RIPng on DST
    ripNg.Set ("SplitHorizon", EnumValue (RipNg::POISON_REVERSE));

    InternetStackHelper stack;
    stack.SetRoutingHelper(ripNg);
    stack.Install(A);
    stack.Install(B);
    stack.Install(C);
    stack.Install(D);

    InternetStackHelper stackNoRouting;
    stackNoRouting.Install(SRC);
    stackNoRouting.Install(DST);

    // Assign IPv6 addresses to each network
    Ipv6AddressHelper ipv6;

    ipv6.SetBase("2001:1::", 64);
    Ipv6InterfaceContainer i_SRC_A = ipv6.Assign(d_SRC_A);

    ipv6.SetBase("2001:2::", 64);
    Ipv6InterfaceContainer i_A_B   = ipv6.Assign(d_A_B);

    ipv6.SetBase("2001:3::", 64);
    Ipv6InterfaceContainer i_A_C   = ipv6.Assign(d_A_C);

    ipv6.SetBase("2001:4::", 64);
    Ipv6InterfaceContainer i_B_C   = ipv6.Assign(d_B_C);

    ipv6.SetBase("2001:5::", 64);
    Ipv6InterfaceContainer i_B_D   = ipv6.Assign(d_B_D);

    ipv6.SetBase("2001:6::", 64);
    Ipv6InterfaceContainer i_C_D   = ipv6.Assign(d_C_D);

    ipv6.SetBase("2001:7::", 64);
    Ipv6InterfaceContainer i_D_DST = ipv6.Assign(d_D_DST);

    for (uint32_t i = 0; i < i_SRC_A.GetN (); ++i)
        i_SRC_A.SetForwarding(i, true);
    for (uint32_t i = 0; i < i_A_B.GetN (); ++i)
        i_A_B.SetForwarding(i, true);
    for (uint32_t i = 0; i < i_A_C.GetN (); ++i)
        i_A_C.SetForwarding(i, true);
    for (uint32_t i = 0; i < i_B_C.GetN (); ++i)
        i_B_C.SetForwarding(i, true);
    for (uint32_t i = 0; i < i_B_D.GetN (); ++i)
        i_B_D.SetForwarding(i, true);
    for (uint32_t i = 0; i < i_C_D.GetN (); ++i)
        i_C_D.SetForwarding(i, true);
    for (uint32_t i = 0; i < i_D_DST.GetN (); ++i)
        i_D_DST.SetForwarding(i, true);

    // Set interfaces UP
    for (uint32_t i = 0; i < i_SRC_A.GetN (); ++i)
        i_SRC_A.SetDefaultRouteInAllNodes (0);
    for (uint32_t i = 0; i < i_A_B.GetN (); ++i)
        i_A_B.SetDefaultRouteInAllNodes (0);
    for (uint32_t i = 0; i < i_A_C.GetN (); ++i)
        i_A_C.SetDefaultRouteInAllNodes (0);
    for (uint32_t i = 0; i < i_B_C.GetN (); ++i)
        i_B_C.SetDefaultRouteInAllNodes (0);
    for (uint32_t i = 0; i < i_B_D.GetN (); ++i)
        i_B_D.SetDefaultRouteInAllNodes (0);
    for (uint32_t i = 0; i < i_C_D.GetN (); ++i)
        i_C_D.SetDefaultRouteInAllNodes (0);
    for (uint32_t i = 0; i < i_D_DST.GetN (); ++i)
        i_D_DST.SetDefaultRouteInAllNodes (0);

    // Set link metric for C-D link to 10 on both ends
    Ptr<Ipv6> ipv6C = C->GetObject<Ipv6> ();
    Ptr<Ipv6> ipv6D = D->GetObject<Ipv6> ();
    Ptr<RipNg> ripngC = C->GetObject<RipNg> ();
    Ptr<RipNg> ripngD = D->GetObject<RipNg> ();

    uint32_t c_c_d = ipv6C->GetInterfaceForDevice (d_C_D.Get (0));
    uint32_t d_c_d = ipv6D->GetInterfaceForDevice (d_C_D.Get (1));
    if (ripngC && ripngD)
    {
        ripngC->SetInterfaceMetric (c_c_d, 10);
        ripngD->SetInterfaceMetric (d_c_d, 10);
    }

    // Assign static addresses for A and D
    // Let's use additional addresses 2001:100::1/128 and 2001:200::1/128 for A and D loopbacks.
    Ptr<Ipv6> ipv6A = A->GetObject<Ipv6> ();
    Ptr<Ipv6> ipv6D2 = D->GetObject<Ipv6> ();
    int32_t loA = ipv6A->AddInterface (CreateObject<LoopbackNetDevice> ());
    ipv6A->AddAddress (loA, Ipv6InterfaceAddress (Ipv6Address ("2001:100::1"), 128));
    ipv6A->SetMetric (loA, 1);
    ipv6A->SetUp (loA);

    int32_t loD = ipv6D2->AddInterface (CreateObject<LoopbackNetDevice> ());
    ipv6D2->AddAddress (loD, Ipv6InterfaceAddress (Ipv6Address ("2001:200::1"), 128));
    ipv6D2->SetMetric (loD, 1);
    ipv6D2->SetUp (loD);

    // SRC uses static routing to reach DST's address via A
    Ipv6StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv6StaticRouting> staticRoutingSrc = staticRoutingHelper.GetStaticRouting(SRC->GetObject<Ipv6> ());

    staticRoutingSrc->SetDefaultRoute(i_SRC_A.GetAddress(0,1), 1);

    // DST sets default route towards D
    Ptr<Ipv6StaticRouting> staticRoutingDst = staticRoutingHelper.GetStaticRouting(DST->GetObject<Ipv6> ());
    staticRoutingDst->SetDefaultRoute(i_D_DST.GetAddress(1,1), 1);

    // Schedule applications and events
    uint16_t port = 9;
    V6PingHelper ping6(i_D_DST.GetAddress(1,1));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Verbose", BooleanValue(true));
    ping6.SetAttribute("Size", UintegerValue(56));
    ping6.SetAttribute("Count", UintegerValue(5));
    ApplicationContainer app = ping6.Install(SRC);
    app.Start(Seconds(3.0));
    app.Stop(Seconds(60.0));

    // Tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng-topo.tr"));
    p2p.EnablePcapAll("ripng-topo", true);
    p2pCost10.EnablePcapAll("ripng-topo-cost10", true);

    // Simulate link breakage at 40s (bring down B-D link)
    Simulator::Schedule(Seconds(40.0), [&](){
        d_B_D.Get(0)->SetLinkDown();
        d_B_D.Get(1)->SetLinkDown();
    });

    // Simulate link recovery at 44s
    Simulator::Schedule(Seconds(44.0), [&](){
        d_B_D.Get(0)->SetLinkUp();
        d_B_D.Get(1)->SetLinkUp();
    });

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}