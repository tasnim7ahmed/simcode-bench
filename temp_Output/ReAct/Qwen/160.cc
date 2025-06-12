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
    double simulationTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer meshNodesCont;
    meshNodesCont.Create(meshNodes);

    NodeContainer treeNodesCont;
    treeNodesCont.Create(treeNodes);

    PointToPointHelper p2pMesh;
    p2pMesh.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pMesh.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pTree;
    p2pTree.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pTree.SetChannelAttribute("Delay", StringValue("2ms"));

    // Mesh topology: fully connected
    NetDeviceContainer meshDevices[meshNodes][meshNodes];
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = i + 1; j < meshNodes; ++j) {
            NodeContainer pair = NodeContainer(meshNodesCont.Get(i), meshNodesCont.Get(j));
            meshDevices[i][j] = p2pMesh.Install(pair);
        }
    }

    // Tree topology: root (node 0) -> node 1 and 2; node 1 -> node 3
    NetDeviceContainer treeDevices[3];
    treeDevices[0] = p2pTree.Install(NodeContainer(treeNodesCont.Get(0), treeNodesCont.Get(1)));
    treeDevices[1] = p2pTree.Install(NodeContainer(treeNodesCont.Get(0), treeNodesCont.Get(2)));
    treeDevices[2] = p2pTree.Install(NodeContainer(treeNodesCont.Get(1), treeNodesCont.Get(3)));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer meshInterfaces[meshNodes][meshNodes];
    Ipv4InterfaceContainer treeInterfaces[3];

    address.SetBase("10.1.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = i + 1; j < meshNodes; ++j) {
            meshInterfaces[i][j] = address.Assign(meshDevices[i][j]);
            address.NewNetwork();
        }
    }

    for (uint32_t i = 0; i < 3; ++i) {
        treeInterfaces[i] = address.Assign(treeDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Applications
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    onoff.SetConstantRate(DataRate("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer meshApps, treeApps;

    // Mesh traffic: all pairs
    for (uint32_t i = 0; i < meshNodes; ++i) {
        for (uint32_t j = 0; j < meshNodes; ++j) {
            if (i != j) {
                onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(meshInterfaces[0][j].GetAddress(1), port)));
                meshApps.Add(onoff.Install(meshNodesCont.Get(i)));
            }
        }
    }

    // Tree traffic: from leaves to root
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(treeInterfaces[0].GetAddress(0), port)));
    treeApps.Add(onoff.Install(treeNodesCont.Get(1)));
    treeApps.Add(onoff.Install(treeNodesCont.Get(2)));
    treeApps.Add(onoff.Install(treeNodesCont.Get(3)));

    meshApps.Start(Seconds(1.0));
    meshApps.Stop(Seconds(simulationTime - 1));

    treeApps.Start(Seconds(1.0));
    treeApps.Stop(Seconds(simulationTime - 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> meshMonitor = flowmon.Install(meshNodesCont);
    Ptr<FlowMonitor> treeMonitor = flowmon.Install(treeNodesCont);

    // Mobility for animation
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
    mobility.InstallAll();

    // NetAnim
    AnimationInterface anim("topology-comparison.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routes.xml", Seconds(0), Seconds(simulationTime), Seconds(0.25));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    meshMonitor->CheckForLostPackets();
    treeMonitor->CheckForLostPackets();

    std::cout << "\n=== Mesh Topology Results ===\n";
    meshMonitor->SerializeToXmlFile("mesh-output.xml", true, true);
    meshMonitor->Print(std::cout);

    std::cout << "\n=== Tree Topology Results ===\n";
    treeMonitor->SerializeToXmlFile("tree-output.xml", true, true);
    treeMonitor->Print(std::cout);

    Simulator::Destroy();
    return 0;
}