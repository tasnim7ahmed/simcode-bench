#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshVsTreeComparison");

void PrintResults(std::string topology, Ptr<FlowMonitor> flowmon, FlowMonitorHelper &flowmonHelper)
{
    double rxPackets = 0;
    double txPackets = 0;
    double rxBytes = 0;
    double delaySum = 0;
    uint32_t flows = 0;

    flowmon->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        txPackets += i->second.txPackets;
        rxPackets += i->second.rxPackets;
        rxBytes += i->second.rxBytes;
        delaySum += i->second.delaySum.GetSeconds();
        flows++;
    }

    double throughput = (rxBytes * 8.0) / (20.0 * 1000000.0); // Mbps
    double packetDeliveryRatio = rxPackets / txPackets * 100;
    double avgDelay = (rxPackets > 0) ? delaySum / rxPackets : 0;

    std::cout << "** Results for " << topology << " topology **" << std::endl;
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
    std::cout << "Average Latency: " << avgDelay * 1000 << " ms" << std::endl;
    std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << " %" << std::endl << std::endl;
}

int main(int argc, char *argv[])
{
    // Mesh Network
    NodeContainer meshNodes;
    meshNodes.Create(4);

    MobilityHelper meshMobility;
    meshMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(10.0),
                                     "MinY", DoubleValue(10.0),
                                     "DeltaX", DoubleValue(30.0),
                                     "DeltaY", DoubleValue(30.0),
                                     "GridWidth", UintegerValue(2),
                                     "LayoutType", StringValue("RowFirst"));
    meshMobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                  "PositionAllocator", PointerValue(meshMobility.GetPositionAllocator()));
    meshMobility.Install(meshNodes);

    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = mesh.Install(WifiPhyHelper::Default(), meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper meshIp;
    meshIp.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshIfaces = meshIp.Assign(meshDevices);

    // Tree Network (star-like, but as tree)
    NodeContainer treeNodes;
    treeNodes.Create(4);
    InternetStackHelper internet2;
    internet2.Install(treeNodes);

    MobilityHelper treeMobility;
    Ptr<ListPositionAllocator> treePos = CreateObject<ListPositionAllocator>();
    treePos->Add(Vector(80, 10, 0));   // root
    treePos->Add(Vector(60, 30, 0));   // child 1
    treePos->Add(Vector(100, 30, 0));  // child 2
    treePos->Add(Vector(90, 50, 0));   // child 3
    treeMobility.SetPositionAllocator(treePos);
    treeMobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                                  "PositionAllocator", PointerValue(treePos));
    treeMobility.Install(treeNodes);

    // Create tree links (0 is root, 1/2/3 are children)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(treeNodes.Get(0), treeNodes.Get(1));
    NetDeviceContainer d02 = p2p.Install(treeNodes.Get(0), treeNodes.Get(2));
    NetDeviceContainer d13 = p2p.Install(treeNodes.Get(1), treeNodes.Get(3)); // to make a tree that's not star

    Ipv4AddressHelper treeAddr;
    treeAddr.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = treeAddr.Assign(d01);
    treeAddr.NewNetwork();
    Ipv4InterfaceContainer i02 = treeAddr.Assign(d02);
    treeAddr.NewNetwork();
    Ipv4InterfaceContainer i13 = treeAddr.Assign(d13);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Traffic on Mesh: node 0 -> node 3
    uint16_t meshPort = 9000;
    UdpServerHelper meshServer(meshPort);
    ApplicationContainer meshServerApp = meshServer.Install(meshNodes.Get(3));
    meshServerApp.Start(Seconds(1.0));
    meshServerApp.Stop(Seconds(20.0));

    UdpClientHelper meshClient(meshIfaces.GetAddress(3), meshPort);
    meshClient.SetAttribute("MaxPackets", UintegerValue(32000));
    meshClient.SetAttribute("Interval", TimeValue(MilliSeconds(5)));
    meshClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer meshClientApp = meshClient.Install(meshNodes.Get(0));
    meshClientApp.Start(Seconds(2.0));
    meshClientApp.Stop(Seconds(19.0));

    // UDP Traffic on Tree: node 0 -> node 3 (path: 0->1->3)
    uint16_t treePort = 9001;
    UdpServerHelper treeServer(treePort);
    ApplicationContainer treeServerApp = treeServer.Install(treeNodes.Get(3));
    treeServerApp.Start(Seconds(1.0));
    treeServerApp.Stop(Seconds(20.0));

    Ipv4Address treeDst = i13.GetAddress(1); // node 3's address on link to 1
    UdpClientHelper treeClient(treeDst, treePort);
    treeClient.SetAttribute("MaxPackets", UintegerValue(32000));
    treeClient.SetAttribute("Interval", TimeValue(MilliSeconds(5)));
    treeClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer treeClientApp = treeClient.Install(treeNodes.Get(0));
    treeClientApp.Start(Seconds(2.0));
    treeClientApp.Stop(Seconds(19.0));

    // FlowMonitors
    FlowMonitorHelper flowmonMesh;
    Ptr<FlowMonitor> monitorMesh = flowmonMesh.Install(meshNodes);

    FlowMonitorHelper flowmonTree;
    Ptr<FlowMonitor> monitorTree = flowmonTree.Install(treeNodes);

    // NetAnim
    AnimationInterface anim("mesh-tree-anim.xml");
    // Set different colors for groups
    anim.SetConstantPosition(meshNodes.Get(0), 10, 10);
    anim.SetConstantPosition(meshNodes.Get(1), 40, 10);
    anim.SetConstantPosition(meshNodes.Get(2), 10, 40);
    anim.SetConstantPosition(meshNodes.Get(3), 40, 40);
    for (uint32_t i=0; i<4; ++i) anim.UpdateNodeColor(meshNodes.Get(i), 255, 0, 0); // Mesh: red

    anim.SetConstantPosition(treeNodes.Get(0), 80, 10);
    anim.SetConstantPosition(treeNodes.Get(1), 60, 30);
    anim.SetConstantPosition(treeNodes.Get(2), 100, 30);
    anim.SetConstantPosition(treeNodes.Get(3), 90, 50);
    for (uint32_t i=0; i<4; ++i) anim.UpdateNodeColor(treeNodes.Get(i), 0, 0, 255); // Tree: blue

    // Animate traffic
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    std::cout << "=============== FINAL RESULTS ===============" << std::endl;
    PrintResults("Mesh", monitorMesh, flowmonMesh);
    PrintResults("Tree", monitorTree, flowmonTree);

    Simulator::Destroy();
    return 0;
}