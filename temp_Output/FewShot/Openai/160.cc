#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void PrintResults(std::string topology, FlowMonitorHelper& flowHelper, Ptr<FlowMonitor> monitor) {
    monitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    double totalTxBytes = 0;
    double totalRxBytes = 0;
    uint64_t totalLostPackets = 0;
    double delaySum = 0;
    uint32_t rxPackets = 0;

    std::cout << "*** Results for " << topology << " topology ***" << std::endl;

    for (auto const& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = flowHelper.GetClassifier()->FindFlow(flow.first);

        double throughput = (flow.second.rxBytes * 8.0) / (20.0 * 1000000.0); // Mbps
        double avgDelay = flow.second.rxPackets > 0 ? flow.second.delaySum.GetSeconds() / flow.second.rxPackets : 0.0;
        double pdr = flow.second.txPackets > 0 ? (double)flow.second.rxPackets / (double)flow.second.txPackets : 0.0;

        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flow.second.lostPackets << std::endl;
        std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
        std::cout << "  Avg. Delay: " << avgDelay * 1000 << " ms" << std::endl;
        std::cout << "  PDR: " << pdr * 100 << " %" << std::endl << std::endl;

        totalTxBytes += flow.second.txBytes;
        totalRxBytes += flow.second.rxBytes;
        totalLostPackets += flow.second.lostPackets;
        delaySum += flow.second.delaySum.GetSeconds();
        rxPackets += flow.second.rxPackets;
    }

    double totalThroughput = (totalRxBytes * 8.0) / (20.0 * 1000000.0); // Mbps
    double avgDelay = rxPackets > 0 ? delaySum / rxPackets : 0.0;
    double totalPdr = totalTxBytes > 0 ? (double)totalRxBytes / (double)totalTxBytes : 0.0;

    std::cout << "Aggregate Results for " << topology << " topology:\n";
    std::cout << "  Total Throughput: " << totalThroughput << " Mbps\n";
    std::cout << "  Average Delay: " << avgDelay * 1000 << " ms\n";
    std::cout << "  Packet Delivery Ratio: " << totalPdr * 100 << " %\n" << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable Logging for applications (optional)
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer treeNodes;
    treeNodes.Create(4);

    // Internet stacks
    InternetStackHelper stack;
    stack.Install(meshNodes);
    stack.Install(treeNodes);

    // Mesh topology: Complete graph
    std::vector<NetDeviceContainer> meshDevices;
    std::vector<Ipv4InterfaceContainer> meshInterfaces;
    PointToPointHelper p2pMesh;
    p2pMesh.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pMesh.SetChannelAttribute("Delay", StringValue("2ms"));

    // All possible pairs for mesh (4 nodes, 6 links)
    int meshLink = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            NetDeviceContainer link = p2pMesh.Install(meshNodes.Get(i), meshNodes.Get(j));
            meshDevices.push_back(link);

            std::ostringstream subnet;
            subnet << "10.1." << meshLink << ".0";
            Ipv4AddressHelper address;
            address.SetBase(subnet.str().c_str(), "255.255.255.0");
            meshInterfaces.push_back(address.Assign(link));
            ++meshLink;
        }
    }

    // Tree topology (binary tree; Node0 is root, Node1 & Node2 are children, Node3 child of Node1)
    PointToPointHelper p2pTree;
    p2pTree.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pTree.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer treeDev01 = p2pTree.Install(treeNodes.Get(0), treeNodes.Get(1));
    NetDeviceContainer treeDev02 = p2pTree.Install(treeNodes.Get(0), treeNodes.Get(2));
    NetDeviceContainer treeDev13 = p2pTree.Install(treeNodes.Get(1), treeNodes.Get(3));

    // Assign IPs
    Ipv4AddressHelper treeAddr;
    treeAddr.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer treeIf01 = treeAddr.Assign(treeDev01);

    treeAddr.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer treeIf02 = treeAddr.Assign(treeDev02);

    treeAddr.SetBase("10.2.2.0", "255.255.255.0");
    Ipv4InterfaceContainer treeIf13 = treeAddr.Assign(treeDev13);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Applications --- Mesh: Node0 sends to Node3
    uint16_t meshPort = 8000;
    UdpServerHelper meshServer(meshPort);
    ApplicationContainer meshServerApp = meshServer.Install(meshNodes.Get(3));
    meshServerApp.Start(Seconds(1.0));
    meshServerApp.Stop(Seconds(20.0));

    UdpClientHelper meshClient(meshInterfaces[2].GetAddress(1), meshPort); // 0-3 link is interfaces[2]
    meshClient.SetAttribute("MaxPackets", UintegerValue(10000));
    meshClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    meshClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer meshClientApp = meshClient.Install(meshNodes.Get(0));
    meshClientApp.Start(Seconds(2.0));
    meshClientApp.Stop(Seconds(20.0));

    // UDP Applications --- Tree: Node0 sends to Node3
    uint16_t treePort = 9000;
    UdpServerHelper treeServer(treePort);
    ApplicationContainer treeServerApp = treeServer.Install(treeNodes.Get(3));
    treeServerApp.Start(Seconds(1.0));
    treeServerApp.Stop(Seconds(20.0));

    UdpClientHelper treeClient(treeIf13.GetAddress(1), treePort); // Node3's address in tree
    treeClient.SetAttribute("MaxPackets", UintegerValue(10000));
    treeClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    treeClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer treeClientApp = treeClient.Install(treeNodes.Get(0));
    treeClientApp.Start(Seconds(2.0));
    treeClientApp.Stop(Seconds(20.0));

    // NetAnim setup
    AnimationInterface anim("topology-comparison.xml");
    // Mesh Node Positions
    anim.SetConstantPosition(meshNodes.Get(0), 10, 40);
    anim.SetConstantPosition(meshNodes.Get(1), 30, 60);
    anim.SetConstantPosition(meshNodes.Get(2), 50, 40);
    anim.SetConstantPosition(meshNodes.Get(3), 30, 20);
    // Tree Node Positions
    anim.SetConstantPosition(treeNodes.Get(0), 100, 40); // root
    anim.SetConstantPosition(treeNodes.Get(1), 120, 60); // child1
    anim.SetConstantPosition(treeNodes.Get(2), 120, 20); // child2
    anim.SetConstantPosition(treeNodes.Get(3), 140, 60); // child of Node1

    // Animate packet flow (Node descriptions)
    anim.UpdateNodeDescription(meshNodes.Get(0), "MeshNode0");
    anim.UpdateNodeDescription(meshNodes.Get(1), "MeshNode1");
    anim.UpdateNodeDescription(meshNodes.Get(2), "MeshNode2");
    anim.UpdateNodeDescription(meshNodes.Get(3), "MeshNode3");
    anim.UpdateNodeDescription(treeNodes.Get(0), "TreeNode0");
    anim.UpdateNodeDescription(treeNodes.Get(1), "TreeNode1");
    anim.UpdateNodeDescription(treeNodes.Get(2), "TreeNode2");
    anim.UpdateNodeDescription(treeNodes.Get(3), "TreeNode3");

    // Animating traffic events for mesh and tree
    anim.EnablePacketMetadata(true);

    // Node movement (for visualization, not affecting topology)
    Ptr<ConstantPositionMobilityModel> mobility;
    mobility = meshNodes.Get(3)->GetObject<ConstantPositionMobilityModel>();
    Simulator::Schedule(Seconds(5.0), &ConstantPositionMobilityModel::SetPosition, mobility, Vector(30,10,0));
    Simulator::Schedule(Seconds(15.0), &ConstantPositionMobilityModel::SetPosition, mobility, Vector(30,20,0));

    mobility = treeNodes.Get(3)->GetObject<ConstantPositionMobilityModel>();
    Simulator::Schedule(Seconds(8.0), &ConstantPositionMobilityModel::SetPosition, mobility, Vector(150,60,0));
    Simulator::Schedule(Seconds(16.0), &ConstantPositionMobilityModel::SetPosition, mobility, Vector(140,60,0));

    // FlowMonitor for mesh
    FlowMonitorHelper meshFlowHelper;
    Ptr<FlowMonitor> meshMonitor = meshFlowHelper.Install(meshNodes);

    // FlowMonitor for tree
    FlowMonitorHelper treeFlowHelper;
    Ptr<FlowMonitor> treeMonitor = treeFlowHelper.Install(treeNodes);

    Simulator::Stop(Seconds(20.1));
    Simulator::Run();

    // Results
    std::cout << "\n---------- Simulation Results ----------\n";
    PrintResults("Mesh", meshFlowHelper, meshMonitor);
    PrintResults("Tree", treeFlowHelper, treeMonitor);

    Simulator::Destroy();
    return 0;
}