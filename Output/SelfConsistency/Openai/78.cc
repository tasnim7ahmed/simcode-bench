/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation: RIP topology with 6 nodes (SRC, DST, A, B, C, D), variable link costs, 
 * split horizon with poison reverse, CLI options for verbosity and table/ping output, 
 * and scheduled link failure & recovery.
 *
 *    SRC---A---B---D---DST
 *           \   |   /
 *            C-------
 *
 * Link costs: cost=1 everywhere except C-D, which is 10.
 * 
 * Author: ns-3 example
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/rip-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipCustomTopologyExample");

void
PrintRoutingTableAt(Time printTime, Ptr<Node> node, bool printRoutes)
{
    if (!printRoutes) return;
    Ipv4GlobalRoutingHelper g;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    std::ostringstream oss;
    ipv4->GetRoutingProtocol()->Print(oss, Simulator::Now().GetSeconds());
    std::cout << "Routing table of node " << node->GetId() << " (" << node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << ") at " << printTime.GetSeconds() << "s" << std::endl;
    std::cout << oss.str() << std::endl;
}

void
PingRtt (std::string context, Time rtt)
{
    std::cout << context << " Ping RTT: " << rtt.GetMilliSeconds() << " ms" << std::endl;
}

int 
main(int argc, char *argv[])
{
    bool verbose = false;
    bool printRoutes = false;
    bool showPing = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Enable all log components", verbose);
    cmd.AddValue("printRoutes", "Print routing tables", printRoutes);
    cmd.AddValue("showPing", "Show ping application results", showPing);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnableAll(LOG_PREFIX_TIME);
        LogComponentEnable("Rip", LOG_LEVEL_INFO);
        LogComponentEnable("RipCustomTopologyExample", LOG_LEVEL_INFO);
    }

    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(6); // 0: SRC, 1: DST, 2: A, 3: B, 4: C, 5: D

    // Define Node names
    Ptr<Node> src = nodes.Get(0);
    Ptr<Node> dst = nodes.Get(1);
    Ptr<Node> a   = nodes.Get(2);
    Ptr<Node> b   = nodes.Get(3);
    Ptr<Node> c   = nodes.Get(4);
    Ptr<Node> d   = nodes.Get(5);

    Names::Add("SRC", src);
    Names::Add("DST", dst);
    Names::Add("A",   a);
    Names::Add("B",   b);
    Names::Add("C",   c);
    Names::Add("D",   d);

    // Establish node pairs for each point-to-point link
    // We'll make the following links:
    // SRC-A, DST-D, A-B, A-C, B-D, B-C, C-D
    NodeContainer src_a (src, a);
    NodeContainer dst_d (dst, d);
    NodeContainer a_b   (a, b);
    NodeContainer a_c   (a, c);
    NodeContainer b_d   (b, d);
    NodeContainer b_c   (b, c);
    NodeContainer c_d   (c, d);

    // PointToPoint attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and channels
    NetDeviceContainer d_src_a = p2p.Install(src_a);
    NetDeviceContainer d_dst_d = p2p.Install(dst_d);
    NetDeviceContainer d_a_b   = p2p.Install(a_b);
    NetDeviceContainer d_a_c   = p2p.Install(a_c);
    NetDeviceContainer d_b_d   = p2p.Install(b_d);
    NetDeviceContainer d_b_c   = p2p.Install(b_c);

    // For C-D use higher cost
    PointToPointHelper p2p_highCost;
    p2p_highCost.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_highCost.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer d_c_d   = p2p_highCost.Install(c_d);

    // Install Internet stack & RIPv2
    NS_LOG_INFO("Install Internet Stack.");
    RipHelper ripRouting;
    ripRouting.Set("SplitHorizon", StringValue("PoisonReverse"));
    InternetStackHelper stack;
    stack.SetRoutingHelper(ripRouting);
    // Routers: A, B, C, D
    NodeContainer routers;
    routers.Add(a);
    routers.Add(b);
    routers.Add(c);
    routers.Add(d);
    stack.Install(routers);
    // Hosts: SRC, DST (static)
    InternetStackHelper stackHost;
    stackHost.Install(src);
    stackHost.Install(dst);

    // Assign addresses to net devices
    Ipv4AddressHelper ipv4;

    // We assign a /30 subnet for each link
    // For consistency, we use easy-to-read subnets
    std::vector<Ipv4InterfaceContainer> ifs(7);
    int subnet = 1;

    #define ASSIGN_LINK(ipv4, d, subnet) \
        ipv4.SetBase(Ipv4Address(("10.0." + std::to_string(subnet) + ".0").c_str()), "255.255.255.252"); \
        ifs[subnet - 1] = ipv4.Assign(d); \
        subnet++;

    ASSIGN_LINK(ipv4, d_src_a, subnet) // 10.0.1.0/30
    ASSIGN_LINK(ipv4, d_a_b,   subnet) // 10.0.2.0/30
    ASSIGN_LINK(ipv4, d_a_c,   subnet) // 10.0.3.0/30
    ASSIGN_LINK(ipv4, d_b_d,   subnet) // 10.0.4.0/30
    ASSIGN_LINK(ipv4, d_b_c,   subnet) // 10.0.5.0/30
    ASSIGN_LINK(ipv4, d_c_d,   subnet) // 10.0.6.0/30
    ASSIGN_LINK(ipv4, d_dst_d, subnet) // 10.0.7.0/30

    // Set interface metrics (costs)
    // By default, all are 1. Set C-D to cost 10.

    Ptr<Ipv4> ipv4c = c->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4d = d->GetObject<Ipv4>();
    uint32_t ifIndexC = ipv4c->GetInterfaceForDevice(d_c_d.Get(0));
    uint32_t ifIndexD = ipv4d->GetInterfaceForDevice(d_c_d.Get(1));
    ipv4c->SetMetric(ifIndexC, 10);
    ipv4d->SetMetric(ifIndexD, 10);

    // Enable packet forwarding
    for (uint32_t i = 0; i < routers.GetN(); ++i) {
        Ptr<Node> node = routers.Get(i);
        node->GetObject<Ipv4>()->SetAttribute("IpForward", BooleanValue(true));
    }

    // Setup static routing for hosts
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> staticSrc = staticRouting.GetStaticRouting(src->GetObject<Ipv4>());
    Ptr<Ipv4StaticRouting> staticDst = staticRouting.GetStaticRouting(dst->GetObject<Ipv4>());

    // Let SRC send to any via its only interface
    staticSrc->SetDefaultRoute(ifs[0].GetAddress(1), 1);

    // DST's network is only present on D's link 10.0.7.0/30
    // So announce DST as /32 reachable via its direct interface
    staticDst->AddHostRouteTo(
        Ipv4Address("10.0.7.2"), // DST interface address
        Ipv4Address("0.0.0.0"), // Use local host
        1 // interface
    );

    // For routers, announce "own" directly-connected networks, let RIP do the rest
    
    // Install a ping application
    V4PingHelper ping(ifs[6].GetAddress(1)); // DST's IP
    ping.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    ping.SetAttribute("StopTime", TimeValue(Seconds(90.0)));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ApplicationContainer apps = ping.Install(src);
    if (showPing) {
        Config::Connect("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback(&PingRtt));
    }

    // Print routes at 0, 20, 39, 40, 41, 44, and 70 seconds (to see convergence and failure)
    std::vector<double> printTimes = {0.0, 20.0, 39.0, 40.0, 41.0, 44.0, 70.0};
    for (auto t : printTimes) {
        for (uint32_t i = 0; i < routers.GetN(); ++i) {
            Simulator::Schedule(Seconds(t), &PrintRoutingTableAt, Seconds(t), routers.Get(i), printRoutes);
        }
    }

    // Failure at B-D at 40s, recovery at 44s
    Ptr<NetDevice> bToDDev = d_b_d.Get(0); // B's device towards D
    Ptr<NetDevice> dToBDev = d_b_d.Get(1); // D's device towards B

    Simulator::Schedule(Seconds(40.0), &NetDevice::SetDown, bToDDev);
    Simulator::Schedule(Seconds(40.0), &NetDevice::SetDown, dToBDev);
    Simulator::Schedule(Seconds(44.0), &NetDevice::SetUp,   bToDDev);
    Simulator::Schedule(Seconds(44.0), &NetDevice::SetUp,   dToBDev);

    // Good simulation time to let RIP converge on recovery
    Simulator::Stop(Seconds(90.0));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
    return 0;
}