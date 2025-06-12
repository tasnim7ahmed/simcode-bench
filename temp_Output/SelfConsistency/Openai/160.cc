#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshVsTreeComparison");

// Helper function to calculate throughput in Mbps
double CalculateThroughput(uint64_t bytes, double timeSeconds)
{
    return (bytes * 8.0) / (timeSeconds * 1000000.0);
}

// Helper function to get stats from FlowMonitor
struct FlowStats
{
    double throughput;            // Mbps
    double meanDelay;             // ms
    double packetDeliveryRatio;   // unitless [0,1]
};

FlowStats GetFlowStats(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper &flowHelper)
{
    FlowStats stats = {0, 0, 0};
    auto classifier = flowHelper.GetClassifier();
    auto flowStats = flowMonitor->GetFlowStats();

    uint64_t rxPackets = 0, txPackets = 0, rxBytes = 0;
    double totalDelay = 0;
    uint32_t delayCount = 0;
    double startTime = -1, endTime = 0;

    for (const auto &flow : flowStats)
    {
        rxPackets += flow.second.rxPackets;
        txPackets += flow.second.txPackets;
        rxBytes   += flow.second.rxBytes;
        totalDelay += flow.second.delaySum.GetSeconds();
        delayCount += flow.second.rxPackets;

        // Find first packet time and last
        if (flow.second.timeFirstRxPacket.GetSeconds() > 0)
        {
            if (startTime < 0 || flow.second.timeFirstRxPacket.GetSeconds() < startTime)
                startTime = flow.second.timeFirstRxPacket.GetSeconds();
        }

        if (flow.second.timeLastRxPacket.GetSeconds() > endTime)
            endTime = flow.second.timeLastRxPacket.GetSeconds();
    }

    // Throughput
    if (endTime > startTime && startTime >= 0)
        stats.throughput = CalculateThroughput(rxBytes, endTime - startTime);
    else
        stats.throughput = 0.0;

    stats.meanDelay = (delayCount > 0) ? (totalDelay / delayCount * 1000.0) : 0; // ms

    stats.packetDeliveryRatio = (txPackets > 0) ? ((double)rxPackets / (double)txPackets) : 0;

    return stats;
}

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // SIMULATION PARAMETERS
    uint32_t nMesh = 4;
    uint32_t nTree = 4;
    double simTime = 20.0;

    // 1. Create nodes
    NodeContainer meshNodes, treeNodes;
    meshNodes.Create(nMesh);
    treeNodes.Create(nTree);

    // 2. PointToPoint Helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 3. Install Internet stack
    InternetStackHelper stack;
    stack.Install(meshNodes);
    stack.Install(treeNodes);

    // 4. Connect Mesh Topology: Full mesh 4 nodes
    // Node indices: 0,1,2,3
    std::vector<NetDeviceContainer> meshLinks;
    std::vector<Ipv4InterfaceContainer> meshIntfs;
    Ipv4AddressHelper meshAddress;
    meshAddress.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < nMesh; ++i)
    {
        for (uint32_t j = i + 1; j < nMesh; ++j)
        {
            NetDeviceContainer ndc = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
            meshLinks.push_back(ndc);
            meshIntfs.push_back(meshAddress.Assign(ndc));
            meshAddress.NewNetwork();
        }
    }

    // 5. Connect Tree Topology: Node 0 is root, 1-3 are children
    std::vector<NetDeviceContainer> treeLinks;
    std::vector<Ipv4InterfaceContainer> treeIntfs;
    Ipv4AddressHelper treeAddress;
    treeAddress.SetBase("10.2.1.0", "255.255.255.0");

    for (uint32_t i = 1; i < nTree; ++i)
    {
        NetDeviceContainer ndc = p2p.Install(treeNodes.Get(0), treeNodes.Get(i));
        treeLinks.push_back(ndc);
        treeIntfs.push_back(treeAddress.Assign(ndc));
        treeAddress.NewNetwork();
    }

    // 6. Assign IP addresses done above

    // 7. UDP Traffic Applications
    // Each node in mesh and tree: Node 0 sends to Node 3, Node 3 replies back (simple bidirectional test)
    uint16_t meshPort = 9000;
    uint16_t treePort = 9001;

    // Mesh UDP Server on node 3
    UdpServerHelper meshServer(meshPort);
    ApplicationContainer meshServerApp = meshServer.Install(meshNodes.Get(3));
    meshServerApp.Start(Seconds(0.0));
    meshServerApp.Stop(Seconds(simTime));

    // Mesh UDP Client on node 0, send packets to node 3
    // Find correct IP for node 3 from any interface
    Ipv4Address meshDstAddr;
    if (meshIntfs.size() > 0)
        meshDstAddr = meshIntfs[2].GetAddress(1);  // Any interface of node 3 in this small topology

    UdpClientHelper meshClient(meshDstAddr, meshPort);
    meshClient.SetAttribute("MaxPackets", UintegerValue(32000));
    meshClient.SetAttribute("Interval", TimeValue(Seconds(0.01))); // 100 pkt/s
    meshClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer meshClientApp = meshClient.Install(meshNodes.Get(0));
    meshClientApp.Start(Seconds(1.0));
    meshClientApp.Stop(Seconds(simTime - 1));

    // Reply server: Optional, have node 0 run UdpServer, node 3 have client
    UdpServerHelper meshServerReply(meshPort + 1);
    ApplicationContainer meshReplyServerApp = meshServerReply.Install(meshNodes.Get(0));
    meshReplyServerApp.Start(Seconds(0.0));
    meshReplyServerApp.Stop(Seconds(simTime));

    UdpClientHelper meshReplyClient(meshIntfs[0].GetAddress(0), meshPort + 1);
    meshReplyClient.SetAttribute("MaxPackets", UintegerValue(32000));
    meshReplyClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    meshReplyClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer meshReplyClientApp = meshReplyClient.Install(meshNodes.Get(3));
    meshReplyClientApp.Start(Seconds(1.0));
    meshReplyClientApp.Stop(Seconds(simTime - 1));

    // Tree UDP Server on node 3
    UdpServerHelper treeServer(treePort);
    ApplicationContainer treeServerApp = treeServer.Install(treeNodes.Get(3));
    treeServerApp.Start(Seconds(0.0));
    treeServerApp.Stop(Seconds(simTime));

    // Tree UDP Client on node 0, send packets to node 3
    Ipv4Address treeDstAddr = treeIntfs[2].GetAddress(1);

    UdpClientHelper treeClient(treeDstAddr, treePort);
    treeClient.SetAttribute("MaxPackets", UintegerValue(32000));
    treeClient.SetAttribute("Interval", TimeValue(Seconds(0.01))); // 100 pkt/s
    treeClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer treeClientApp = treeClient.Install(treeNodes.Get(0));
    treeClientApp.Start(Seconds(1.0));
    treeClientApp.Stop(Seconds(simTime - 1));

    // Reply server and client for tree, optional for bidirectional
    UdpServerHelper treeServerReply(treePort + 1);
    ApplicationContainer treeReplyServerApp = treeServerReply.Install(treeNodes.Get(0));
    treeReplyServerApp.Start(Seconds(0.0));
    treeReplyServerApp.Stop(Seconds(simTime));

    UdpClientHelper treeReplyClient(treeIntfs[0].GetAddress(0), treePort + 1);
    treeReplyClient.SetAttribute("MaxPackets", UintegerValue(32000));
    treeReplyClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    treeReplyClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer treeReplyClientApp = treeReplyClient.Install(treeNodes.Get(3));
    treeReplyClientApp.Start(Seconds(1.0));
    treeReplyClientApp.Stop(Seconds(simTime - 1));

    // 8. NetAnim Setup
    AnimationInterface anim("mesh-vs-tree.xml");

    // Assign positions for visualization (Mesh: square, Tree: root center + children)
    anim.SetConstantPosition(meshNodes.Get(0), 10, 60);
    anim.SetConstantPosition(meshNodes.Get(1), 60, 60);
    anim.SetConstantPosition(meshNodes.Get(2), 10, 10);
    anim.SetConstantPosition(meshNodes.Get(3), 60, 10);

    anim.SetConstantPosition(treeNodes.Get(0), 120, 35);
    anim.SetConstantPosition(treeNodes.Get(1), 170, 10);
    anim.SetConstantPosition(treeNodes.Get(2), 170, 35);
    anim.SetConstantPosition(treeNodes.Get(3), 170, 60);

    // Animate packet flows: Tag UDP traffic
    anim.EnablePacketMetadata(true);

    // Animate node color (mesh blue, tree green)
    for (uint32_t i = 0; i < meshNodes.GetN(); i++)
        anim.UpdateNodeColor(meshNodes.Get(i), 0, 0, 255); // Blue
    for (uint32_t i = 0; i < treeNodes.GetN(); i++)
        anim.UpdateNodeColor(treeNodes.Get(i), 0, 180, 0); // Green

    // Animate moving nodes (demo only)
    anim.UpdateNodeDescription(meshNodes.Get(0), "Mesh N0");
    anim.UpdateNodeDescription(treeNodes.Get(0), "Tree R");
    anim.ScheduleMove(meshNodes.Get(0), Seconds(5.0), 10, 40);

    // 9. FlowMonitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> meshMonitor = flowHelper.Install(meshNodes);
    Ptr<FlowMonitor> treeMonitor = flowHelper.Install(treeNodes);

    // 10. Run Simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // 11. Gather statistics
    meshMonitor->CheckForLostPackets();
    treeMonitor->CheckForLostPackets();

    // Select only mesh flows (10.x.x.x) and tree flows (10.2.x.x), assuming no address overlap
    FlowStats meshStats = GetFlowStats(meshMonitor, flowHelper);
    FlowStats treeStats = GetFlowStats(treeMonitor, flowHelper);

    std::cout << "\n----- NS-3 Mesh vs Tree Topology Comparison -----" << std::endl;
    std::cout << "Simulation time: " << simTime << "s\n";
    std::cout << " - Throughput (Mbps)\n";
    std::cout << "      Mesh: " << meshStats.throughput << "\n";
    std::cout << "      Tree: " << treeStats.throughput << "\n";
    std::cout << " - Average Latency (ms)\n";
    std::cout << "      Mesh: " << meshStats.meanDelay << "\n";
    std::cout << "      Tree: " << treeStats.meanDelay << "\n";
    std::cout << " - Packet Delivery Ratio\n";
    std::cout << "      Mesh: " << meshStats.packetDeliveryRatio << "\n";
    std::cout << "      Tree: " << treeStats.packetDeliveryRatio << "\n";
    std::cout << "------------------------------------------------\n";
    std::cout << "NetAnim file: mesh-vs-tree.xml\n";
    std::cout << "------------------------------------------------\n";

    // Save FlowMonitor results if needed
    meshMonitor->SerializeToXmlFile("mesh-results.xml", true, true);
    treeMonitor->SerializeToXmlFile("tree-results.xml", true, true);

    Simulator::Destroy();
    return 0;
}