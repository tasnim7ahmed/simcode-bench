/* 
 * Fat-Tree Topology Data Center Network Simulation in ns-3
 * - TCP traffic between servers
 * - NetAnim visualization enabled
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Fat-Tree Parameter: k-ary Fat-Tree (k should be EVEN)
static const uint32_t k = 4; // Number of ports per switch
static const uint32_t pods = k;
static const uint32_t coreSwitches = (k/2)*(k/2);
static const uint32_t aggSwitches = k*k/2;
static const uint32_t edgeSwitches = k*k/2;
static const uint32_t servers = k*k*k/4;

NS_LOG_COMPONENT_DEFINE("FatTreeDataCenter");

int 
main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Logging
    LogComponentEnable("FatTreeDataCenter", LOG_LEVEL_INFO);

    // Containers
    NodeContainer coreSwitchNodes, aggSwitchNodes, edgeSwitchNodes, serverNodes;
    
    coreSwitchNodes.Create(coreSwitches);
    aggSwitchNodes.Create(aggSwitches);
    edgeSwitchNodes.Create(edgeSwitches);
    serverNodes.Create(servers);

    // Install Internet stacks
    InternetStackHelper internet;
    internet.Install(coreSwitchNodes);
    internet.Install(aggSwitchNodes);
    internet.Install(edgeSwitchNodes);
    internet.Install(serverNodes);

    // Helpers
    PointToPointHelper p2pCoreToAgg, p2pAggToEdge, p2pEdgeToServer;
    p2pCoreToAgg.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2pCoreToAgg.SetChannelAttribute ("Delay", StringValue ("2ms"));

    p2pAggToEdge.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2pAggToEdge.SetChannelAttribute ("Delay", StringValue ("1ms"));

    p2pEdgeToServer.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2pEdgeToServer.SetChannelAttribute ("Delay", StringValue ("500us"));

    // Keep track of all devices/interfaces for IP assignment
    std::vector<NetDeviceContainer> edgeToServerDevices;
    std::vector<NetDeviceContainer> edgeToAggDevices;
    std::vector<NetDeviceContainer> aggToCoreDevices;

    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    uint32_t subnet = 1;

    // ------- Connect Edge Switches to Servers -----
    for (uint32_t e = 0; e < edgeSwitches; ++e)
    {
        for (uint32_t h = 0; h < k/2; ++h)
        {
            uint32_t hostIdx = (e * (k/2)) + h;
            NetDeviceContainer link = p2pEdgeToServer.Install(edgeSwitchNodes.Get(e), serverNodes.Get(hostIdx));
            edgeToServerDevices.push_back(link);

            std::ostringstream subnetStr;
            subnetStr << "10." << pods << "." << e << ".0";
            address.SetBase(Ipv4Address(subnetStr.str().c_str()), "255.255.255.0");
            address.Assign(link);
            ++subnet;
        }
    }

    // ------- Connect Edge Switches to Aggregation Switches in Pods -----
    // In each pod:
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        // pod offset
        uint32_t edgeOffset = pod * (k/2);
        uint32_t aggOffset = pod * (k/2);
        for (uint32_t e = 0; e < k/2; ++e)
        {
            for (uint32_t a = 0; a < k/2; ++a)
            {
                NetDeviceContainer link = p2pAggToEdge.Install(aggSwitchNodes.Get(aggOffset + a), edgeSwitchNodes.Get(edgeOffset + e));
                edgeToAggDevices.push_back(link);

                std::ostringstream subnetStr;
                subnetStr << "10." << pod << "." << e << "." << (a + 128);
                address.SetBase(Ipv4Address(subnetStr.str().c_str()), "255.255.255.0");
                address.Assign(link);
                ++subnet;
            }
        }
    }

    // ------- Connect Aggregation Switches to Core Switches -----
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        uint32_t aggOffset = pod * (k/2);
        for (uint32_t a = 0; a < k/2; ++a)
        {
            // Define which core switches to connect:
            for (uint32_t c = 0; c < k/2; ++c)
            {
                uint32_t coreIdx = a * (k/2) + c;
                NetDeviceContainer link = p2pCoreToAgg.Install(coreSwitchNodes.Get(coreIdx), aggSwitchNodes.Get(aggOffset + a));
                aggToCoreDevices.push_back(link);

                std::ostringstream subnetStr;
                subnetStr << "10." << 255 << "." << a << "." << coreIdx;
                address.SetBase(Ipv4Address(subnetStr.str().c_str()), "255.255.255.0");
                address.Assign(link);
                ++subnet;
            }
        }
    }

    // Assign IPv4 addresses to all nodes (if not yet assigned)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---- Application Layer: TCP Flows between random server pairs ---- //
    uint16_t port = 5000;
    double appStart = 1.0, appStop = 9.0;

    for (uint32_t i = 0; i < servers/2; ++i)
    {
        uint32_t srcIdx = i;
        uint32_t dstIdx = servers-i-1;

        // Sink on dst server
        Address sinkAddr (InetSocketAddress (serverNodes.Get(dstIdx)->GetObject<Ipv4>()->GetAddress (1,0).GetLocal (), port));
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
        ApplicationContainer sinkApp = sinkHelper.Install(serverNodes.Get(dstIdx));
        sinkApp.Start (Seconds (appStart));
        sinkApp.Stop (Seconds (appStop+1.0));

        // OnOff on src server
        OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddr);
        clientHelper.SetAttribute ("DataRate", StringValue("500Mbps"));
        clientHelper.SetAttribute ("PacketSize", UintegerValue(1024));
        clientHelper.SetAttribute ("StartTime", TimeValue(Seconds(appStart)));
        clientHelper.SetAttribute ("StopTime", TimeValue(Seconds(appStop)));
        ApplicationContainer clientApp = clientHelper.Install(serverNodes.Get(srcIdx));
    }

    // --------- NetAnim setup ---------- //
    AnimationInterface anim ("fat-tree-datacenter.xml");

    // Assign positions for NetAnim (simple grid-like placement)
    uint32_t row = 0, col = 0;
    // Core switches (top row)
    for (uint32_t i = 0; i < coreSwitches; ++i)
    {
        anim.SetConstantPosition(coreSwitchNodes.Get(i), 200 + i*75, 50);
    }
    // Aggregation switches (row below core)
    for (uint32_t i = 0; i < aggSwitches; ++i)
    {
        anim.SetConstantPosition(aggSwitchNodes.Get(i), 100 + i*40, 150);
    }
    // Edge switches (row below agg)
    for (uint32_t i = 0; i < edgeSwitches; ++i)
    {
        anim.SetConstantPosition(edgeSwitchNodes.Get(i), 100 + i*40, 250);
    }
    // Servers (bottom row)
    for (uint32_t i = 0; i < servers; ++i)
    {
        anim.SetConstantPosition(serverNodes.Get(i), 80 + i*15, 350);
    }

    // Run simulation
    Simulator::Stop(Seconds(appStop+2.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}