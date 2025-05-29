#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeDataCenterSimulation");

int main(int argc, char *argv[])
{
    // Fat-Tree parameter K (must be even); k=4 gives 16 servers
    uint32_t k = 4;
    uint32_t pods = k;
    uint32_t aggPerPod = k/2;
    uint32_t edgePerPod = k/2;
    uint32_t coreSwitches = (k/2)*(k/2);
    uint32_t serversPerEdge = k/2;
    uint32_t nServers = (k*k*k)/4;

    // Storage for nodes
    NodeContainer coreSwitchNodes;
    coreSwitchNodes.Create(coreSwitches);

    std::vector<NodeContainer> aggSwitchNodes(pods);
    std::vector<NodeContainer> edgeSwitchNodes(pods);
    std::vector<NodeContainer> serverNodes(pods * edgePerPod);

    for(uint32_t pod=0; pod<pods; ++pod) {
        aggSwitchNodes[pod].Create(aggPerPod);
        edgeSwitchNodes[pod].Create(edgePerPod);
        for(uint32_t e=0; e<edgePerPod; ++e) {
            serverNodes[pod*edgePerPod + e].Create(serversPerEdge);
        }
    }

    // Point-to-Point configurations
    PointToPointHelper p2pCoreToAgg, p2pAggToEdge, p2pEdgeToServer;
    p2pCoreToAgg.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pCoreToAgg.SetChannelAttribute("Delay", StringValue("2ms"));
    p2pAggToEdge.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pAggToEdge.SetChannelAttribute("Delay", StringValue("1ms"));
    p2pEdgeToServer.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2pEdgeToServer.SetChannelAttribute("Delay", StringValue("0.5ms"));

    InternetStackHelper stack;

    // Install stack (core+agg+edge first)
    stack.Install(coreSwitchNodes);
    for(uint32_t pod=0; pod<pods; ++pod) {
        stack.Install(aggSwitchNodes[pod]);
        stack.Install(edgeSwitchNodes[pod]);
        for(uint32_t e=0; e<edgePerPod; ++e) {
            stack.Install(serverNodes[pod*edgePerPod + e]);
        }
    }

    // All NetDevices, needed for NetAnim
    NodeContainer allNodes;
    allNodes.Add(coreSwitchNodes);
    for(uint32_t pod=0; pod<pods; ++pod) {
        allNodes.Add(aggSwitchNodes[pod]);
        allNodes.Add(edgeSwitchNodes[pod]);
        for(uint32_t e=0; e<edgePerPod; ++e) {
            allNodes.Add(serverNodes[pod*edgePerPod + e]);
        }
    }

    // Keep track of all NetDeviceContainers for possible animation/tracing
    std::vector<NetDeviceContainer> allDevices;

    // Core <-> Agg connections
    // Each core switch connects to each pod, one agg per pod
    for(uint32_t c = 0; c < coreSwitches; ++c) {
        uint32_t group = c/(k/2);
        for(uint32_t pod = 0; pod < pods; ++pod) {
            NetDeviceContainer ndc = p2pCoreToAgg.Install(coreSwitchNodes.Get(c), aggSwitchNodes[pod].Get(group));
            allDevices.push_back(ndc);
        }
    }

    // Agg <-> Edge connections (within the same pod)
    for(uint32_t pod=0; pod<pods; ++pod) {
        for(uint32_t agg=0; agg<aggPerPod; ++agg) {
            for(uint32_t edge=0; edge<edgePerPod; ++edge) {
                NetDeviceContainer ndc = p2pAggToEdge.Install(aggSwitchNodes[pod].Get(agg), edgeSwitchNodes[pod].Get(edge));
                allDevices.push_back(ndc);
            }
        }
    }

    // Edge <-> Server connections
    for(uint32_t pod=0; pod<pods; ++pod) {
        for(uint32_t edge=0; edge<edgePerPod; ++edge) {
            for(uint32_t s=0; s<serversPerEdge; ++s) {
                NetDeviceContainer ndc = p2pEdgeToServer.Install(edgeSwitchNodes[pod].Get(edge), serverNodes[pod*edgePerPod + edge].Get(s));
                allDevices.push_back(ndc);
            }
        }
    }

    // IP address assignment
    Ipv4AddressHelper ipv4;
    char network[16];
    std::vector<Ipv4InterfaceContainer> serverInterfaces;

    // For simplicity, use pod.edge as 10.pod.edge.0/24 for servers (unique per edge)
    for(uint32_t pod=0; pod<pods; ++pod) {
        for(uint32_t edge=0; edge<edgePerPod; ++edge) {
            sprintf(network, "10.%u.%u.0", pod, edge);
            ipv4.SetBase(network, "255.255.255.0");
            for(uint32_t s=0; s<serversPerEdge; ++s) {
                // Only the server device is assigned here (since edge switches are routers)
                NetDeviceContainer ndc;
                ndc.Add(serverNodes[pod*edgePerPod + edge].Get(s)->GetDevice(0));
                Ipv4InterfaceContainer iface = ipv4.Assign(ndc);
                if(s == 0) ipv4.NewNetwork(); // For next device on same LAN
                if(serverInterfaces.size() <= pod*edgePerPod + edge)
                    serverInterfaces.resize(pod*edgePerPod + edge + 1);
                serverInterfaces[pod*edgePerPod + edge].Add(iface);
            }
        }
    }

    // Complete IP address assignment for all other links (assign but don't keep)
    ipv4.SetBase("172.16.0.0", "255.255.255.0");
    for(auto& ndc : allDevices) {
        ipv4.Assign(ndc);
        ipv4.NewNetwork();
    }

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Choose some servers as sources and sinks
    uint16_t port = 5000;
    ApplicationContainer sinks, sources;

    // For demonstration, make all servers in pod0-edge0 send to all servers in last pod+edge
    std::vector<Ipv4Address> destAddresses;
    for(uint32_t s=0; s<serversPerEdge; ++s) {
        Ipv4Address addr = serverInterfaces[(pods-1)*edgePerPod + (edgePerPod-1)].GetAddress(s, 0);
        destAddresses.push_back(addr);
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(addr, port));
        sinks.Add(sinkHelper.Install(serverNodes[(pods-1)*edgePerPod + (edgePerPod-1)].Get(s)));
    }

    sinks.Start(Seconds(0.8));
    sinks.Stop(Seconds(10.0));

    // Each source connects to one sink
    for(uint32_t s=0; s<serversPerEdge; ++s) {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(destAddresses[s], port));
        onoff.SetAttribute("DataRate", StringValue("400Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1448));
        onoff.SetAttribute("MaxBytes", UintegerValue(20000000));
        sources.Add(onoff.Install(serverNodes[0].Get(s)));
    }

    sources.Start(Seconds(1.0));
    sources.Stop(Seconds(9.9));

    // NetAnim configuration
    AnimationInterface anim("fattree-datacenter.xml");

    // Lay out: (optional) Slightly space out switches and servers
    uint32_t nodeId = 0;
    double yBase = 100.0;
    // Core
    for(uint32_t c=0; c<coreSwitches; ++c,++nodeId)
        anim.SetConstantPosition(coreSwitchNodes.Get(c), 100 + 70*c, yBase);
    yBase += 80;
    // Agg
    for(uint32_t pod=0; pod<pods; ++pod)
        for(uint32_t agg=0; agg<aggPerPod; ++agg,++nodeId)
            anim.SetConstantPosition(aggSwitchNodes[pod].Get(agg), 40 + pod*180 + agg*60, yBase);
    yBase += 80;
    // Edge
    for(uint32_t pod=0; pod<pods; ++pod)
        for(uint32_t edge=0; edge<edgePerPod; ++edge,++nodeId)
            anim.SetConstantPosition(edgeSwitchNodes[pod].Get(edge), 40 + pod*180 + edge*60, yBase);
    yBase += 60;
    // Servers
    for(uint32_t pod=0; pod<pods; ++pod)
        for(uint32_t edge=0; edge<edgePerPod; ++edge)
            for(uint32_t s=0; s<serversPerEdge; ++s,++nodeId)
                anim.SetConstantPosition(serverNodes[pod*edgePerPod+edge].Get(s), 40 + pod*180 + edge*60 + s*10, yBase);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}