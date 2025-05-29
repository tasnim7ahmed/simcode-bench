#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Fat-tree parameter: k-ary fat-tree
const uint32_t k = 4; // number of ports per switch (k should be even and >=2)
const uint32_t numPods = k;
const uint32_t numCore = (k/2)*(k/2);
const uint32_t numAggrPerPod = k/2;
const uint32_t numEdgePerPod = k/2;
const uint32_t numAggr = numPods * numAggrPerPod;
const uint32_t numEdge = numPods * numEdgePerPod;
const uint32_t numServersPerEdge = k/2;
const uint32_t numServers = numPods * numEdgePerPod * numServersPerEdge;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create all nodes
    NodeContainer coreSwitches, aggrSwitches, edgeSwitches, servers;
    coreSwitches.Create(numCore);
    aggrSwitches.Create(numAggr);
    edgeSwitches.Create(numEdge);
    servers.Create(numServers);

    InternetStackHelper stack;
    stack.Install(coreSwitches);
    stack.Install(aggrSwitches);
    stack.Install(edgeSwitches);
    stack.Install(servers);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // IP address allocation helpers (per subnet)
    std::vector<Ipv4AddressHelper> addressHelpers;
    uint32_t subnetCount = 0;

    // Connect core to aggregation
    std::vector<NetDeviceContainer> coreToAggrLinks;
    for (uint32_t pod = 0; pod < numPods; ++pod) {
        for (uint32_t aggr = 0; aggr < numAggrPerPod; ++aggr) {
            for (uint32_t c = 0; c < k/2; ++c) {
                uint32_t core = aggr*(k/2) + c;
                NetDeviceContainer link = p2p.Install(coreSwitches.Get(core), aggrSwitches.Get(pod*numAggrPerPod + aggr));

                Ipv4AddressHelper address;
                address.SetBase("10.100." + std::to_string(subnetCount) + ".0", "255.255.255.0");
                address.Assign(link);
                subnetCount++;
                coreToAggrLinks.push_back(link);
            }
        }
    }

    // Connect aggregation to edge
    std::vector<NetDeviceContainer> aggrToEdgeLinks;
    for (uint32_t pod = 0; pod < numPods; ++pod) {
        for (uint32_t aggr = 0; aggr < numAggrPerPod; ++aggr) {
            for (uint32_t edge = 0; edge < numEdgePerPod; ++edge) {
                NetDeviceContainer link = p2p.Install(aggrSwitches.Get(pod*numAggrPerPod + aggr), edgeSwitches.Get(pod*numEdgePerPod + edge));

                Ipv4AddressHelper address;
                address.SetBase("10.101." + std::to_string(subnetCount) + ".0", "255.255.255.0");
                address.Assign(link);
                subnetCount++;
                aggrToEdgeLinks.push_back(link);
            }
        }
    }

    // Connect edge to servers
    std::vector<NetDeviceContainer> edgeToServerLinks;
    for (uint32_t pod = 0; pod < numPods; ++pod) {
        for (uint32_t edge = 0; edge < numEdgePerPod; ++edge) {
            for (uint32_t s = 0; s < numServersPerEdge; ++s) {
                uint32_t serverIdx = (pod*numEdgePerPod + edge)*numServersPerEdge + s;
                NetDeviceContainer link = p2p.Install(edgeSwitches.Get(pod*numEdgePerPod + edge), servers.Get(serverIdx));

                Ipv4AddressHelper address;
                address.SetBase("10.102." + std::to_string(subnetCount) + ".0", "255.255.255.0");
                address.Assign(link);
                subnetCount++;
                edgeToServerLinks.push_back(link);
            }
        }
    }

    // Assign IP addresses to all nodes (all are already assigned automatically)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP applications between servers
    uint16_t port = 50000;
    ApplicationContainer serverApps, clientApps;
    double appStart = 1.0;
    double appStop = 9.0;

    // Let every server send to the next (wrap around)
    for (uint32_t i = 0; i < numServers; ++i) {
        uint32_t dstIdx = (i + 1) % numServers;

        // On destination: TCP packet sink
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(servers.Get(dstIdx));
        sinkApp.Start(Seconds(appStart));
        sinkApp.Stop(Seconds(appStop));
        serverApps.Add(sinkApp);

        // On source: BulkSend
        // Find server dst IP
        Ptr<Ipv4> ipv4 = servers.Get(dstIdx)->GetObject<Ipv4>();
        Ipv4Address ip = ipv4->GetAddress(1,0).GetLocal();

        BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(ip, port));
        bulkSend.SetAttribute("MaxBytes", UintegerValue(1024 * 1024)); // 1 MB per server
        ApplicationContainer clientApp = bulkSend.Install(servers.Get(i));
        clientApp.Start(Seconds(appStart+0.1)); // Slight offset
        clientApp.Stop(Seconds(appStop));
        clientApps.Add(clientApp);
    }

    // NetAnim visualization
    AnimationInterface anim("fat-tree-netanim.xml");

    // Layout: assign positions for visualization
    double yStep = 50.0;
    double xStep = 20.0;
    // Core
    for (uint32_t i = 0; i < numCore; ++i) {
        anim.SetConstantPosition(coreSwitches.Get(i), 50 + xStep*(i-0.5*numCore), 0);
    }
    // Aggregation
    for (uint32_t i = 0; i < numAggr; ++i) {
        anim.SetConstantPosition(aggrSwitches.Get(i), 20 + xStep*i, yStep*1);
    }
    // Edge
    for (uint32_t i = 0; i < numEdge; ++i) {
        anim.SetConstantPosition(edgeSwitches.Get(i), 20 + xStep*i, yStep*2);
    }
    // Servers
    for (uint32_t i = 0; i < numServers; ++i) {
        anim.SetConstantPosition(servers.Get(i), 10 + xStep*i, yStep*3);
    }

    Simulator::Stop(Seconds(appStop+1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}