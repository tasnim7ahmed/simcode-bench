#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TopologyComparison");

void CreateMesh(Ptr<Node> n0, Ptr<Node> n1, Ptr<Node> n2, Ptr<Node> n3, NodeContainer& meshNodes, NetDeviceContainer& meshDevices, Ipv4InterfaceContainer& meshInterfaces) {
    meshNodes.Add(n0); meshNodes.Add(n1); meshNodes.Add(n2); meshNodes.Add(n3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // All possible connections
    NetDeviceContainer d01 = p2p.Install(n0, n1);
    NetDeviceContainer d02 = p2p.Install(n0, n2);
    NetDeviceContainer d03 = p2p.Install(n0, n3);
    NetDeviceContainer d12 = p2p.Install(n1, n2);
    NetDeviceContainer d13 = p2p.Install(n1, n3);
    NetDeviceContainer d23 = p2p.Install(n2, n3);

    meshDevices.Add(d01); meshDevices.Add(d02); meshDevices.Add(d03);
    meshDevices.Add(d12); meshDevices.Add(d13); meshDevices.Add(d23);

    InternetStackHelper stack;
    stack.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    meshInterfaces.Add(address.Assign(d01));
    address.NewNetwork();
    meshInterfaces.Add(address.Assign(d02));
    address.NewNetwork();
    meshInterfaces.Add(address.Assign(d03));
    address.NewNetwork();
    meshInterfaces.Add(address.Assign(d12));
    address.NewNetwork();
    meshInterfaces.Add(address.Assign(d13));
    address.NewNetwork();
    meshInterfaces.Add(address.Assign(d23));
}

void CreateTree(Ptr<Node> root, Ptr<Node> child1, Ptr<Node> child2, Ptr<Node> leaf, NodeContainer& treeNodes, NetDeviceContainer& treeDevices, Ipv4InterfaceContainer& treeInterfaces) {
    treeNodes.Add(root); treeNodes.Add(child1); treeNodes.Add(child2); treeNodes.Add(leaf);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dRootChild1 = p2p.Install(root, child1);
    NetDeviceContainer dRootChild2 = p2p.Install(root, child2);
    NetDeviceContainer dChild1Leaf = p2p.Install(child1, leaf);

    treeDevices.Add(dRootChild1); treeDevices.Add(dRootChild2); treeDevices.Add(dChild1Leaf);

    InternetStackHelper stack;
    stack.Install(treeNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.2.1.0", "255.255.255.0");
    treeInterfaces.Add(address.Assign(dRootChild1));
    address.NewNetwork();
    treeInterfaces.Add(address.Assign(dRootChild2));
    address.NewNetwork();
    treeInterfaces.Add(address.Assign(dChild1Leaf));
}

int main(int argc, char *argv[]) {
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer meshNodes;
    NodeContainer treeNodes;

    Ptr<Node> meshN0 = CreateObject<Node>();
    Ptr<Node> meshN1 = CreateObject<Node>();
    Ptr<Node> meshN2 = CreateObject<Node>();
    Ptr<Node> meshN3 = CreateObject<Node>();

    Ptr<Node> treeRoot = CreateObject<Node>();
    Ptr<Node> treeChild1 = CreateObject<Node>();
    Ptr<Node> treeChild2 = CreateObject<Node>();
    Ptr<Node> treeLeaf = CreateObject<Node>();

    NetDeviceContainer meshDevices;
    NetDeviceContainer treeDevices;

    Ipv4InterfaceContainer meshInterfaces;
    Ipv4InterfaceContainer treeInterfaces;

    CreateMesh(meshN0, meshN1, meshN2, meshN3, meshNodes, meshDevices, meshInterfaces);
    CreateTree(treeRoot, treeChild1, treeChild2, treeLeaf, treeNodes, treeDevices, treeInterfaces);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(meshNodes);

    mobility.Install(treeNodes);

    AnimationInterface anim("topology-comparison.xml");

    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(meshNodes.Get(i), "Mesh Node");
        anim.UpdateNodeColor(meshNodes.Get(i), 255, 0, 0);
    }

    for (uint32_t i = 0; i < treeNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(treeNodes.Get(i), "Tree Node");
        anim.UpdateNodeColor(treeNodes.Get(i), 0, 0, 255);
    }

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverAppsMesh = echoServer.Install(meshN3);
    serverAppsMesh.Start(Seconds(1.0));
    serverAppsMesh.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClientMesh(meshInterfaces.GetAddress(5, 1), 9);
    echoClientMesh.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClientMesh.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClientMesh.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsMesh = echoClientMesh.Install(meshN0);
    clientAppsMesh.Start(Seconds(2.0));
    clientAppsMesh.Stop(Seconds(20.0));

    ApplicationContainer serverAppsTree = echoServer.Install(treeLeaf);
    serverAppsTree.Start(Seconds(1.0));
    serverAppsTree.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClientTree(treeInterfaces.GetAddress(2, 1), 9);
    echoClientTree.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClientTree.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClientTree.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsTree = echoClientTree.Install(treeRoot);
    clientAppsTree.Start(Seconds(2.0));
    clientAppsTree.Stop(Seconds(20.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double meshThroughput = 0, meshLatency = 0, meshPdr = 0;
    uint32_t meshFlows = 0;

    double treeThroughput = 0, treeLatency = 0, treePdr = 0;
    uint32_t treeFlows = 0;

    for (auto &[id, flow] : stats) {
        Ipv4Address srcAddr, dstAddr;
        flow.sourceAddress.GetObject<Ipv4>()->GetAddress(1, Ipv4InterfaceAddress());
        flow.destinationAddress.GetObject<Ipv4>()->GetAddress(1, Ipv4InterfaceAddress());

        if (flow.txPackets > 0) {
            double pdr = static_cast<double>(flow.rxPackets) / flow.txPackets;
            double throughput = static_cast<double>(flow.rxBytes * 8) / (flow.timeLastRxPacket.GetSeconds() - flow.timeFirstTxPacket.GetSeconds()) / 1000; // Kbps
            double latency = flow.delaySum.GetSeconds() / flow.rxPackets;

            if (srcAddr == meshInterfaces.GetAddress(0, 0)) {
                meshThroughput += throughput;
                meshLatency += latency;
                meshPdr += pdr;
                meshFlows++;
            } else {
                treeThroughput += throughput;
                treeLatency += latency;
                treePdr += pdr;
                treeFlows++;
            }
        }
    }

    if (meshFlows > 0) {
        meshThroughput /= meshFlows;
        meshLatency /= meshFlows;
        meshPdr /= meshFlows;
    }

    if (treeFlows > 0) {
        treeThroughput /= treeFlows;
        treeLatency /= treeFlows;
        treePdr /= treeFlows;
    }

    std::cout << "\n--- Topology Comparison Results ---" << std::endl;
    std::cout << "Mesh Topology: Throughput=" << meshThroughput << " Kbps, Latency=" << meshLatency << " s, PDR=" << meshPdr << std::endl;
    std::cout << "Tree Topology: Throughput=" << treeThroughput << " Kbps, Latency=" << treeLatency << " s, PDR=" << treePdr << std::endl;

    Simulator::Destroy();
    return 0;
}