#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mesh-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void SetNodePositions(Ptr<Node> node, double x, double y)
{
    Ptr<ConstantPositionMobilityModel> mob = node->GetObject<ConstantPositionMobilityModel>();
    if (!mob)
    {
        mob = CreateObject<ConstantPositionMobilityModel>();
        node->AggregateObject(mob);
    }
    mob->SetPosition(Vector(x, y, 0.0));
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t packetSize = 1024;
    uint32_t nPacketsPerSec = 100;
    double simTime = 20.0;

    // Mesh Topology
    NodeContainer meshNodes;
    meshNodes.Create(4);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

    // Assign mobility for visualization (square layout)
    Ptr<ListPositionAllocator> meshAllocator = CreateObject<ListPositionAllocator>();
    meshAllocator->Add(Vector(10,10,0));
    meshAllocator->Add(Vector(20,10,0));
    meshAllocator->Add(Vector(10,20,0));
    meshAllocator->Add(Vector(20,20,0));
    MobilityHelper meshMobility;
    meshMobility.SetPositionAllocator(meshAllocator);
    meshMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    meshMobility.Install(meshNodes);

    // Tree Topology (Star to approximate tree for 4 nodes)
    NodeContainer treeNodes;
    treeNodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    NetDeviceContainer treeDevices = csma.Install(treeNodes);

    Ptr<ListPositionAllocator> treeAllocator = CreateObject<ListPositionAllocator>();
    treeAllocator->Add(Vector(50,15,0)); // root
    treeAllocator->Add(Vector(60,5,0));  // child 1
    treeAllocator->Add(Vector(60,15,0)); // child 2
    treeAllocator->Add(Vector(60,25,0)); // child 3
    MobilityHelper treeMobility;
    treeMobility.SetPositionAllocator(treeAllocator);
    treeMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    treeMobility.Install(treeNodes);

    // Stack
    InternetStackHelper stack;
    stack.Install(meshNodes);
    stack.Install(treeNodes);

    // Assign IP addresses
    Ipv4AddressHelper meshIp;
    meshIp.SetBase("10.1.1.0","255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = meshIp.Assign(meshDevices);

    Ipv4AddressHelper treeIp;
    treeIp.SetBase("10.2.1.0","255.255.255.0");
    Ipv4InterfaceContainer treeInterfaces = treeIp.Assign(treeDevices);

    // UDP Apps for Mesh
    uint16_t meshPort = 4000;
    UdpServerHelper meshServer(meshPort);
    ApplicationContainer meshServerApp = meshServer.Install(meshNodes.Get(0));
    meshServerApp.Start(Seconds(1.0));
    meshServerApp.Stop(Seconds(simTime));

    UdpClientHelper meshClient(meshInterfaces.GetAddress(0), meshPort);
    meshClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    meshClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / nPacketsPerSec)));
    meshClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer meshClientApp = meshClient.Install(meshNodes.Get(3));
    meshClientApp.Start(Seconds(2.0));
    meshClientApp.Stop(Seconds(simTime));

    // UDP Apps for Tree
    uint16_t treePort = 5000;
    UdpServerHelper treeServer(treePort);
    ApplicationContainer treeServerApp = treeServer.Install(treeNodes.Get(0));
    treeServerApp.Start(Seconds(1.0));
    treeServerApp.Stop(Seconds(simTime));

    UdpClientHelper treeClient(treeInterfaces.GetAddress(0), treePort);
    treeClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    treeClient.SetAttribute("Interval", TimeValue(Seconds(1.0 / nPacketsPerSec)));
    treeClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer treeClientApp = treeClient.Install(treeNodes.Get(3));
    treeClientApp.Start(Seconds(2.0));
    treeClientApp.Stop(Seconds(simTime));

    // Animate Packet Traces
    AnimationInterface anim("mesh-tree-netanim.xml");
    anim.SetBackgroundColor(0.9,0.9,0.9);

    // Mesh labels
    anim.UpdateNodeDescription(meshNodes.Get(0),"Mesh-0");
    anim.UpdateNodeDescription(meshNodes.Get(1),"Mesh-1");
    anim.UpdateNodeDescription(meshNodes.Get(2),"Mesh-2");
    anim.UpdateNodeDescription(meshNodes.Get(3),"Mesh-3");
    anim.UpdateNodeColor(meshNodes.Get(0), 0, 255, 0);
    anim.UpdateNodeColor(meshNodes.Get(1), 0, 128, 255);
    anim.UpdateNodeColor(meshNodes.Get(2), 255, 128, 0);
    anim.UpdateNodeColor(meshNodes.Get(3), 255, 0, 0);

    // Tree labels
    anim.UpdateNodeDescription(treeNodes.Get(0),"Tree-0");
    anim.UpdateNodeDescription(treeNodes.Get(1),"Tree-1");
    anim.UpdateNodeDescription(treeNodes.Get(2),"Tree-2");
    anim.UpdateNodeDescription(treeNodes.Get(3),"Tree-3");
    anim.UpdateNodeColor(treeNodes.Get(0), 200, 255, 200);
    anim.UpdateNodeColor(treeNodes.Get(1), 200, 200, 255);
    anim.UpdateNodeColor(treeNodes.Get(2), 255, 200, 200);
    anim.UpdateNodeColor(treeNodes.Get(3), 255, 200, 0);

    // Animate traffic
    anim.EnablePacketMetadata(true);

    // Animate mesh node movement
    anim.ScheduleMoveMesh(meshNodes.Get(1), Seconds(15), Vector(25,15,0));

    // Animate tree node movement
    anim.ScheduleMoveNode(treeNodes.Get(2), Seconds(7), Vector(65,20,0));

    // FlowMonitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simTime+1));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    double mesh_rxBytes = 0, mesh_txBytes = 0;
    double mesh_delaySum = 0, mesh_rxPackets = 0, mesh_txPackets = 0;

    double tree_rxBytes = 0, tree_txBytes = 0;
    double tree_delaySum = 0, tree_rxPackets = 0, tree_txPackets = 0;

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if ((t.sourceAddress >= Ipv4Address("10.1.1.0")) && (t.sourceAddress <= Ipv4Address("10.1.1.255"))) // Mesh
        {
            mesh_txPackets += flow.second.txPackets;
            mesh_rxPackets += flow.second.rxPackets;
            mesh_rxBytes += flow.second.rxBytes;
            mesh_txBytes += flow.second.txBytes;
            mesh_delaySum += flow.second.delaySum.GetSeconds();
        }
        else if ((t.sourceAddress >= Ipv4Address("10.2.1.0")) && (t.sourceAddress <= Ipv4Address("10.2.1.255"))) // Tree
        {
            tree_txPackets += flow.second.txPackets;
            tree_rxPackets += flow.second.rxPackets;
            tree_rxBytes += flow.second.rxBytes;
            tree_txBytes += flow.second.txBytes;
            tree_delaySum += flow.second.delaySum.GetSeconds();
        }
    }

    double meshThroughput = (mesh_rxBytes *8) / (simTime*1000000.0); // Mbps
    double meshPktDelivery = mesh_rxPackets / mesh_txPackets;
    double meshAvgDelay = mesh_rxPackets > 0 ? mesh_delaySum / mesh_rxPackets : 0;

    double treeThroughput = (tree_rxBytes *8) / (simTime*1000000.0); // Mbps
    double treePktDelivery = tree_rxPackets / tree_txPackets;
    double treeAvgDelay = tree_rxPackets > 0 ? tree_delaySum / tree_rxPackets : 0;

    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "Performance Comparison: Mesh vs Tree Topology" << std::endl;
    std::cout << "Simulation Time: " << simTime << "s " << std::endl << std::endl;

    std::cout << "Mesh Topology: " << std::endl;
    std::cout << "  Throughput: " << meshThroughput << " Mbps" << std::endl;
    std::cout << "  Avg Latency: " << meshAvgDelay*1000 << " ms" << std::endl;
    std::cout << "  Packet Delivery Ratio: " << meshPktDelivery*100 << " %" << std::endl << std::endl;

    std::cout << "Tree Topology: " << std::endl;
    std::cout << "  Throughput: " << treeThroughput << " Mbps" << std::endl;
    std::cout << "  Avg Latency: " << treeAvgDelay*1000 << " ms" << std::endl;
    std::cout << "  Packet Delivery Ratio: " << treePktDelivery*100 << " %" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    Simulator::Destroy();
    return 0;
}