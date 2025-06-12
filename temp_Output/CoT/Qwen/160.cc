#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TopologyComparison");

int main(int argc, char *argv[]) {
    uint32_t meshNodes = 4;
    uint32_t treeNodes = 4;
    double simTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer meshNodesContainer;
    meshNodesContainer.Create(meshNodes);

    NodeContainer treeNodesContainer;
    treeNodesContainer.Create(treeNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    // Mesh topology
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = i + 1; j < meshNodes; ++j) {
            NetDeviceContainer devices = p2p.Install(NodeContainer(meshNodesContainer.Get(i), meshNodesContainer.Get(j)));
            address.SetBase("10.1." + std::to_string(i * meshNodes + j) + ".0", "255.255.255.0");
            interfaces.push_back(address.Assign(devices));
        }
    }

    // Tree topology (root at node 0)
    NetDeviceContainer treeDevices[treeNodes - 1];
    Ipv4InterfaceContainer treeInterfaces[treeNodes - 1];

    address.SetBase("10.2.0.0", "255.255.255.0");
    treeDevices[0] = p2p.Install(NodeContainer(treeNodesContainer.Get(0), treeNodesContainer.Get(1)));
    treeInterfaces[0] = address.Assign(treeDevices[0]);
    address.NewNetwork();
    treeDevices[1] = p2p.Install(NodeContainer(treeNodesContainer.Get(0), treeNodesContainer.Get(2)));
    treeInterfaces[1] = address.Assign(treeDevices[1]);
    address.NewNetwork();
    treeDevices[2] = p2p.Install(NodeContainer(treeNodesContainer.Get(0), treeNodesContainer.Get(3)));
    treeInterfaces[2] = address.Assign(treeDevices[2]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    // Mesh: Nodes 0 and 3 communicate
    serverApps = echoServer.Install(meshNodesContainer.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClient(interfaces[5].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(2000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = echoClient.Install(meshNodesContainer.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Tree: Node 1 sends to Node 0
    serverApps = echoServer.Install(treeNodesContainer.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClientTree(treeInterfaces[0].GetAddress(0), port);
    echoClientTree.SetAttribute("MaxPackets", UintegerValue(2000));
    echoClientTree.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClientTree.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = echoClientTree.Install(treeNodesContainer.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Mobility for animation
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    AnimationInterface anim("topology-comparison.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routing-table.xml", Seconds(0), Seconds(simTime), Seconds(0.25));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double meshThroughput = 0.0, meshLatency = 0.0, meshPdr = 0.0;
    double treeThroughput = 0.0, treeLatency = 0.0, treePdr = 0.0;
    uint32_t meshCount = 0, treeCount = 0;

    for (auto &[flowId, flowStats] : stats) {
        Ipv4Address srcAddr, dstAddr;
        for (auto &interface : interfaces) {
            if (interface.GetAddress(0) == flowStats.sourceAddress || interface.GetAddress(1) == flowStats.sourceAddress) {
                srcAddr = flowStats.sourceAddress;
            }
            if (interface.GetAddress(0) == flowStats.destinationAddress || interface.GetAddress(1) == flowStats.destinationAddress) {
                dstAddr = flowStats.destinationAddress;
            }
        }

        if (srcAddr == interfaces[0].GetAddress(0) && dstAddr == interfaces[5].GetAddress(1)) {
            meshThroughput += flowStats.rxBytes * 8.0 / simTime / 1000 / 1000;
            meshLatency += flowStats.delaySum.GetSeconds() / flowStats.rxPackets;
            meshPdr += (double)flowStats.rxPackets / (flowStats.txPackets);
            meshCount++;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            if (srcAddr == treeInterfaces[i].GetAddress(1) && dstAddr == treeInterfaces[i].GetAddress(0)) {
                treeThroughput += flowStats.rxBytes * 8.0 / simTime / 1000 / 1000;
                treeLatency += flowStats.delaySum.GetSeconds() / flowStats.rxPackets;
                treePdr += (double)flowStats.rxPackets / (flowStats.txPackets);
                treeCount++;
            }
        }
    }

    if (meshCount > 0) {
        meshThroughput /= meshCount;
        meshLatency /= meshCount;
        meshPdr /= meshCount;
    }

    if (treeCount > 0) {
        treeThroughput /= treeCount;
        treeLatency /= treeCount;
        treePdr /= treeCount;
    }

    std::cout << "\n--- Topology Comparison Results ---" << std::endl;
    std::cout << "Mesh Throughput: " << meshThroughput << " Mbps | Latency: " << meshLatency << " s | PDR: " << meshPdr << std::endl;
    std::cout << "Tree Throughput: " << treeThroughput << " Mbps | Latency: " << treeLatency << " s | PDR: " << treePdr << std::endl;

    Simulator::Destroy();
    return 0;
}