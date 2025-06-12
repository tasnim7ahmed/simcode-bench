#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    uint32_t packetSize = 1024;
    uint32_t numPackets = 400;
    double interval = 0.05;
    double simulationTime = 20.0;

    // Create Nodes for Mesh and Tree
    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer treeNodes;
    treeNodes.Create(4);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(meshNodes);
    internet.Install(treeNodes);

    // Mesh Topology Setup
    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    // Mobility for Mesh Nodes
    MobilityHelper meshMobility;
    meshMobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(50.0), "DeltaY", DoubleValue(50.0),
        "GridWidth", UintegerValue(2), "LayoutType", StringValue("RowFirst"));
    meshMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0, 150, 0, 150)));
    meshMobility.Install(meshNodes);

    // Assign IP Addresses to Mesh
    Ipv4AddressHelper meshAddress;
    meshAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = meshAddress.Assign(meshDevices);

    // Tree Topology Setup
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Tree: Node 0 is root, connects to 1, 2, 3
    NetDeviceContainer d1 = p2p.Install(treeNodes.Get(0), treeNodes.Get(1));
    NetDeviceContainer d2 = p2p.Install(treeNodes.Get(0), treeNodes.Get(2));
    NetDeviceContainer d3 = p2p.Install(treeNodes.Get(0), treeNodes.Get(3));

    // Mobility for Tree
    MobilityHelper treeMobility;
    treeMobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(200.0), "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(50.0), "DeltaY", DoubleValue(50.0),
        "GridWidth", UintegerValue(2), "LayoutType", StringValue("RowFirst"));
    treeMobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue(treeMobility.GetPositionAllocator()));
    treeMobility.Install(treeNodes);

    // Assign IP addresses to Tree
    Ipv4AddressHelper treeAddress;
    treeAddress.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = treeAddress.Assign(d1);
    treeAddress.NewNetwork();
    Ipv4InterfaceContainer i2 = treeAddress.Assign(d2);
    treeAddress.NewNetwork();
    Ipv4InterfaceContainer i3 = treeAddress.Assign(d3);

    // Traffic Applications: Mesh
    // UDP Server on meshNodes.Get(0)
    uint16_t meshPort = 4000;
    UdpServerHelper meshServer(meshPort);
    ApplicationContainer meshServerApps = meshServer.Install(meshNodes.Get(0));
    meshServerApps.Start(Seconds(1.0));
    meshServerApps.Stop(Seconds(simulationTime));

    // UDP Clients on meshNodes.Get(1..3)
    for (uint32_t i = 1; i < 4; ++i)
    {
        UdpClientHelper meshClient(meshInterfaces.GetAddress(0), meshPort);
        meshClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
        meshClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        meshClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer meshClientApp = meshClient.Install(meshNodes.Get(i));
        meshClientApp.Start(Seconds(2.0 + i));
        meshClientApp.Stop(Seconds(simulationTime));
    }

    // Traffic Applications: Tree
    uint16_t treePort = 5000;
    UdpServerHelper treeServer(treePort);
    ApplicationContainer treeServerApps = treeServer.Install(treeNodes.Get(0));
    treeServerApps.Start(Seconds(1.0));
    treeServerApps.Stop(Seconds(simulationTime));

    for (uint32_t i = 1; i < 4; ++i)
    {
        Ipv4Address dstAddr;
        if (i == 1) dstAddr = i1.GetAddress(0); // server = node 0
        if (i == 2) dstAddr = i2.GetAddress(0);
        if (i == 3) dstAddr = i3.GetAddress(0);
        UdpClientHelper treeClient(dstAddr, treePort);
        treeClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
        treeClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        treeClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer treeClientApp = treeClient.Install(treeNodes.Get(i));
        treeClientApp.Start(Seconds(2.0 + i));
        treeClientApp.Stop(Seconds(simulationTime));
    }

    // NetAnim setup
    AnimationInterface anim("mesh-tree-netanim.xml");

    // Mesh nodes: color blue
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
        anim.UpdateNodeColor(meshNodes.Get(i), 0, 0, 255);

    // Tree nodes: color red
    for (uint32_t i = 0; i < treeNodes.GetN(); ++i)
        anim.UpdateNodeColor(treeNodes.Get(i), 255, 0, 0);

    // Set label for mesh and tree
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
        anim.UpdateNodeDescription(meshNodes.Get(i), "Mesh" + std::to_string(i));
    for (uint32_t i = 0; i < treeNodes.GetN(); ++i)
        anim.UpdateNodeDescription(treeNodes.Get(i), "Tree" + std::to_string(i));

    anim.EnablePacketMetadata(true);

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    // Collect and output results
    double meshThroughput = 0, meshDelay = 0, meshDelivered = 0, meshTotal = 0;
    double treeThroughput = 0, treeDelay = 0, treeDelivered = 0, treeTotal = 0;
    int meshFlows = 0, treeFlows = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto s = stats.begin(); s != stats.end(); ++s)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(s->first);

        bool isMesh = (t.sourceAddress.IsEqual(meshInterfaces.GetAddress(1)) ||
                       t.sourceAddress.IsEqual(meshInterfaces.GetAddress(2)) ||
                       t.sourceAddress.IsEqual(meshInterfaces.GetAddress(3)));
        bool isTree = (t.sourceAddress.IsEqual(i1.GetAddress(1)) ||
                       t.sourceAddress.IsEqual(i2.GetAddress(1)) ||
                       t.sourceAddress.IsEqual(i3.GetAddress(1)));

        double throughput = s->second.rxBytes * 8.0 / simulationTime / 1e6;
        double avgDelay = s->second.delaySum.GetSeconds() / s->second.rxPackets;
        double pdr = s->second.rxPackets*1.0 / s->second.txPackets;

        if (isMesh)
        {
            meshThroughput += throughput;
            meshDelay += (s->second.rxPackets > 0 ? avgDelay : 0);
            meshDelivered += s->second.rxPackets;
            meshTotal += s->second.txPackets;
            meshFlows++;
        }
        else if (isTree)
        {
            treeThroughput += throughput;
            treeDelay += (s->second.rxPackets > 0 ? avgDelay : 0);
            treeDelivered += s->second.rxPackets;
            treeTotal += s->second.txPackets;
            treeFlows++;
        }
    }

    std::cout << "------ Simulation Results over " << simulationTime << " seconds ------" << std::endl;

    std::cout << "\nMESH Topology (4 nodes, WiFi Mesh):" << std::endl;
    std::cout << "  Throughput (Mbps)   : " << (meshFlows>0 ? meshThroughput/meshFlows : 0) << std::endl;
    std::cout << "  Average Latency (s) : " << (meshFlows>0 && meshDelivered>0 ? meshDelay/meshFlows : 0) << std::endl;
    std::cout << "  Packet Delivery Ratio: " << (meshTotal>0 ? meshDelivered/meshTotal : 0) << std::endl;

    std::cout << "\nTREE Topology (4 nodes, P2P Tree):" << std::endl;
    std::cout << "  Throughput (Mbps)   : " << (treeFlows>0 ? treeThroughput/treeFlows : 0) << std::endl;
    std::cout << "  Average Latency (s) : " << (treeFlows>0 && treeDelivered>0 ? treeDelay/treeFlows : 0) << std::endl;
    std::cout << "  Packet Delivery Ratio: " << (treeTotal>0 ? treeDelivered/treeTotal : 0) << std::endl;

    Simulator::Destroy();
    return 0;
}