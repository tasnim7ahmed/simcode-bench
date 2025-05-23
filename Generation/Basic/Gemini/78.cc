#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ripv2-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ping-helper.h"
#include <iostream>

void PrintRoutingTables(Ptr<Node> node, const Ripv2Helper& ripRoutingHelper, const std::string& nodeName)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (ipv4)
    {
        Ptr<Ripv2RoutingProtocol> rip = Ptr<Ripv2RoutingProtocol>::DynamicCast(ipv4->GetRoutingProtocol());
        if (rip)
        {
            rip->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout, std::ios::out), Simulator::Now(), nodeName);
        }
    }
}

static bool g_verbose = false;
static bool g_printRoutingTables = false;
static bool g_pingVerbose = false;
static double g_printInterval = 5.0;

int main(int argc, char** argv)
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable logging for various components", g_verbose);
    cmd.AddValue("printRoutingTables", "Enable periodic printing of RIP routing tables", g_printRoutingTables);
    cmd.AddValue("pingVerbose", "Enable verbose output for V4Ping and V4PingEcho applications", g_pingVerbose);
    cmd.AddValue("printInterval", "Interval (seconds) for printing routing tables", g_printInterval);
    cmd.Parse(argc, argv);

    if (g_verbose)
    {
        LogComponentEnable("Ripv2RoutingProtocol", LOG_LEVEL_INFO);
        LogComponentEnable("Ripv2Helper", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv4Interface", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv4RoutingProtocol", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv4EndPoint", LOG_LEVEL_INFO);
        LogComponentEnable("ArpCache", LOG_LEVEL_DEBUG);
    }
    if (g_pingVerbose)
    {
        LogComponentEnable("V4Ping", LOG_LEVEL_INFO);
        LogComponentEnable("V4PingEcho", LOG_LEVEL_INFO);
    }

    NodeContainer srcNode, dstNode;
    srcNode.Create(1);
    dstNode.Create(1);

    NodeContainer routers;
    routers.Create(4); // A, B, C, D
    Ptr<Node> nodeA = routers.Get(0);
    Ptr<Node> nodeB = routers.Get(1);
    Ptr<Node> nodeC = routers.Get(2);
    Ptr<Node> nodeD = routers.Get(3);

    InternetStackHelper internet;
    internet.Install(srcNode);
    internet.Install(dstNode);
    
    // Set Ripv2Helper as the default routing helper for routers
    internet.SetRoutingHelper(Ripv2Helper());
    internet.Install(routers);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper ipv4;

    NetDeviceContainer srcA = p2p.Install(srcNode.Get(0), nodeA);
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer srcAInterfaces = ipv4.Assign(srcA);

    NetDeviceContainer ab = p2p.Install(nodeA, nodeB);
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abInterfaces = ipv4.Assign(ab);

    NetDeviceContainer bc = p2p.Install(nodeB, nodeC);
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcInterfaces = ipv4.Assign(bc);

    NetDeviceContainer bd = p2p.Install(nodeB, nodeD);
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bdInterfaces = ipv4.Assign(bd);

    NetDeviceContainer cd = p2p.Install(nodeC, nodeD);
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer cdInterfaces = ipv4.Assign(cd);

    NetDeviceContainer cdst = p2p.Install(nodeC, dstNode.Get(0));
    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer cdstInterfaces = ipv4.Assign(cdst);

    NetDeviceContainer ddst = p2p.Install(nodeD, dstNode.Get(0));
    ipv4.SetBase("10.0.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ddstInterfaces = ipv4.Assign(ddst);

    Ripv2Helper ripRoutingHelper;
    
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4D = nodeD->GetObject<Ipv4>();

    int32_t c_iface_cd = ipv4C->GetInterfaceForDevice(cd.Get(0));
    int32_t d_iface_cd = ipv4D->GetInterfaceForDevice(cd.Get(1));

    ripRoutingHelper.SetInterfaceMetric(ipv4C, c_iface_cd, 10);
    ripRoutingHelper.SetInterfaceMetric(ipv4D, d_iface_cd, 10);

    Ptr<Ipv4StaticRouting> staticRoutingSrc = Ipv4RoutingHelper::Get<Ipv4StaticRouting>(srcNode.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRoutingSrc->SetDefaultRoute(srcAInterfaces.GetAddress(1), srcNode.Get(0)->GetObject<Ipv4>()->GetInterfaceForDevice(srcA.Get(0)));

    Ptr<Ipv4StaticRouting> staticRoutingDst = Ipv4RoutingHelper::Get<Ipv4StaticRouting>(dstNode.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRoutingDst->SetDefaultRoute(cdstInterfaces.GetAddress(0), dstNode.Get(0)->GetObject<Ipv4>()->GetInterfaceForDevice(cdst.Get(1)));

    PingHelper pingHelper;
    pingHelper.SetSource(srcNode.Get(0)->GetId());
    pingHelper.SetRemote(dstNode.Get(0)->GetId(), cdstInterfaces.GetAddress(1));

    ApplicationContainer pingApps = pingHelper.Install(srcNode.Get(0));
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(58.0));

    Simulator::Schedule(Seconds(40.0), &NetDevice::SetDown, bd.Get(0));
    Simulator::Schedule(Seconds(40.0), &NetDevice::SetDown, bd.Get(1));

    Simulator::Schedule(Seconds(44.0), &NetDevice::SetUp, bd.Get(0));
    Simulator::Schedule(Seconds(44.0), &NetDevice::SetUp, bd.Get(1));

    if (g_printRoutingTables)
    {
        for (double time = 0.0; time <= 60.0; time += g_printInterval)
        {
            Simulator::Schedule(Seconds(time), &PrintRoutingTables, nodeA, ripRoutingHelper, "Node A");
            Simulator::Schedule(Seconds(time), &PrintRoutingTables, nodeB, ripRoutingHelper, "Node B");
            Simulator::Schedule(Seconds(time), &PrintRoutingTables, nodeC, ripRoutingHelper, "Node C");
            Simulator::Schedule(Seconds(time), &PrintRoutingTables, nodeD, ripRoutingHelper, "Node D");
        }
    }

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}