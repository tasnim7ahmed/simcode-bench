#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <vector>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FatTreeDatacenterSimulation");

uint32_t k = 4; // Fat-Tree parameter (can increase for larger topologies)
uint32_t pods;
uint32_t edgeSwitches, aggSwitches, coreSwitches, servers;
uint32_t hostsPerEdge;

// Helper struct
struct FatTreeNodes
{
    NodeContainer core;
    std::vector<NodeContainer> aggregation;
    std::vector<NodeContainer> edge;
    std::vector<NodeContainer> hosts;
};

FatTreeNodes CreateFatTree(uint32_t k)
{
    pods = k;
    edgeSwitches = k;
    aggSwitches = k;
    coreSwitches = (k / 2) * (k / 2);
    hostsPerEdge = k / 2;
    servers = k * k * k / 4;

    FatTreeNodes fatTree;

    fatTree.core.Create(coreSwitches);

    for (uint32_t i = 0; i < pods; ++i)
    {
        NodeContainer agg, edge;
        agg.Create(k / 2);
        edge.Create(k / 2);
        fatTree.aggregation.push_back(agg);
        fatTree.edge.push_back(edge);

        for (uint32_t j = 0; j < k / 2; ++j)
        {
            NodeContainer h;
            h.Create(hostsPerEdge);
            fatTree.hosts.push_back(h);
        }
    }
    return fatTree;
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    CommandLine cmd;
    cmd.AddValue("k", "Fat-Tree parameter", k);
    cmd.Parse(argc, argv);

    // Topology Setup
    FatTreeNodes ft = CreateFatTree(k);

    // Containers for all nodes by type for installation
    NodeContainer allHosts;
    NodeContainer allEdges;
    NodeContainer allAggs;
    NodeContainer allCores;

    for (auto &h : ft.hosts)
        allHosts.Add(h);
    for (auto &e : ft.edge)
        allEdges.Add(e);
    for (auto &a : ft.aggregation)
        allAggs.Add(a);
    allCores = ft.core;

    // PointToPoint Helper
    PointToPointHelper hostToEdge;
    hostToEdge.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    hostToEdge.SetChannelAttribute("Delay", StringValue("2us"));

    PointToPointHelper edgeToAgg;
    edgeToAgg.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    edgeToAgg.SetChannelAttribute("Delay", StringValue("4us"));

    PointToPointHelper aggToCore;
    aggToCore.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    aggToCore.SetChannelAttribute("Delay", StringValue("6us"));

    // Devices/Containers
    std::vector<std::vector<NetDeviceContainer>> hostEdgeLink(pods * (k / 2));
    std::vector<NetDeviceContainer> edgeAggLink;
    std::vector<NetDeviceContainer> aggCoreLink;

    // Host <-> Edge connections
    uint32_t hostIdx = 0;
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t edge = 0; edge < k / 2; ++edge)
        {
            for (uint32_t h = 0; h < hostsPerEdge; ++h, ++hostIdx)
            {
                NetDeviceContainer nd = hostToEdge.Install(ft.hosts[pod * (k / 2) + edge].Get(h), ft.edge[pod].Get(edge));
                hostEdgeLink[pod * (k / 2) + edge].push_back(nd);
            }
        }
    }

    // Edge <-> Aggregation connections
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t edge = 0; edge < k / 2; ++edge)
        {
            for (uint32_t agg = 0; agg < k / 2; ++agg)
            {
                NetDeviceContainer nd = edgeToAgg.Install(ft.edge[pod].Get(edge), ft.aggregation[pod].Get(agg));
                edgeAggLink.push_back(nd);
            }
        }
    }

    // Aggregation <-> Core connections
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t agg = 0; agg < k / 2; ++agg)
        {
            for (uint32_t c = 0; c < k / 2; ++c)
            {
                uint32_t coreIndex = agg * (k / 2) + c;
                NetDeviceContainer nd = aggToCore.Install(ft.aggregation[pod].Get(agg), ft.core.Get(coreIndex));
                aggCoreLink.push_back(nd);
            }
        }
    }

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(allHosts);
    stack.Install(allEdges);
    stack.Install(allAggs);
    stack.Install(allCores);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> hostIpIfaces;
    std::vector<Ipv4InterfaceContainer> edgeAggIpIfaces;
    std::vector<Ipv4InterfaceContainer> aggCoreIpIfaces;

    uint32_t netNum = 0;
    // Host-Edge interfaces
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t edge = 0; edge < k / 2; ++edge)
        {
            for (uint32_t h = 0; h < hostsPerEdge; ++h)
            {
                std::ostringstream subnet;
                subnet << "10." << pod << "." << edge << ".0";
                address.SetBase(subnet.str().c_str(), "255.255.255.0");
                Ipv4InterfaceContainer iface = address.Assign(hostEdgeLink[pod * (k / 2) + edge][h]);
                hostIpIfaces.push_back(iface);
                ++netNum;
            }
        }
    }

    // Edge-Agg interfaces
    netNum = 0;
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t edge = 0; edge < k / 2; ++edge)
        {
            for (uint32_t agg = 0; agg < k / 2; ++agg)
            {
                std::ostringstream subnet;
                subnet << "10." << pod << "." << (edge + (k / 2)) << ".0";
                address.SetBase(subnet.str().c_str(), "255.255.255.0");
                Ipv4InterfaceContainer iface = address.Assign(edgeAggLink[netNum]);
                edgeAggIpIfaces.push_back(iface);
                ++netNum;
            }
        }
    }

    // Agg-Core interfaces
    netNum = 0;
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t agg = 0; agg < k / 2; ++agg)
        {
            for (uint32_t c = 0; c < k / 2; ++c)
            {
                std::ostringstream subnet;
                subnet << "10." << (pods) << "." << netNum << ".0";
                address.SetBase(subnet.str().c_str(), "255.255.255.0");
                Ipv4InterfaceContainer iface = address.Assign(aggCoreLink[netNum]);
                aggCoreIpIfaces.push_back(iface);
                ++netNum;
            }
        }
    }

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications: setup TCP flows between random pairs of hosts
    uint16_t port = 50000;
    double appStart = 0.1;
    double appStop = 7.9;
    uint32_t flowCount = std::min(uint32_t(8), servers);

    for (uint32_t i = 0; i < flowCount; ++i)
    {
        Ptr<Node> src = allHosts.Get(i);
        Ptr<Node> dst = allHosts.Get((i + 1) % servers);

        // Sink on destination
        Address sinkAddress (InetSocketAddress(Ipv4Address::ConvertFrom(dst->GetObject<Ipv4>()->GetAddress(1,0).GetLocal()), port));
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(dst);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        // Source on src
        BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
        source.SetAttribute ("MaxBytes", UintegerValue (50000000));
        ApplicationContainer sourceApp = source.Install(src);
        sourceApp.Start(Seconds(appStart));
        sourceApp.Stop(Seconds(appStop));
        appStart += 0.2;
        appStop += 0.2;
    }

    // NetAnim Setup
    AnimationInterface anim ("fattree-dc.xml");

    // Assign positions for NetAnim
    double hGap = 25.0;
    double yStep = 50.0;
    uint32_t hostCount = 0, pos = 0;

    // Hosts
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t edge = 0; edge < k / 2; ++edge)
        {
            for (uint32_t h = 0; h < hostsPerEdge; ++h, ++hostCount)
            {
                anim.SetConstantPosition(allHosts.Get(hostCount), pos * hGap, 0);
                ++pos;
            }
        }
    }
    // Edge switches
    for (uint32_t i = 0; i < allEdges.GetN(); ++i)
        anim.SetConstantPosition(allEdges.Get(i), i * hGap * 2, yStep);
    // Aggregation switches
    for (uint32_t i = 0; i < allAggs.GetN(); ++i)
        anim.SetConstantPosition(allAggs.Get(i), i * hGap * 2, yStep * 2);
    // Core switches
    for (uint32_t i = 0; i < allCores.GetN(); ++i)
        anim.SetConstantPosition(allCores.Get(i), i * hGap * 3, yStep * 3);

    // Enable NetAnim packet tracing
    anim.EnablePacketMetadata();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}