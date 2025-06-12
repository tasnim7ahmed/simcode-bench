#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Time::SetResolution(Time::NS);
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));

    // Create containers for mesh and tree topologies
    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer treeNodes;
    treeNodes.Create(4);

    // Point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Mesh Topology: Fully connected (each node connected to every other node)
    NetDeviceContainer meshDevices[4][4];
    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            NodeContainer pair(meshNodes.Get(i), meshNodes.Get(j));
            meshDevices[i][j] = p2p.Install(pair);
        }
    }

    // Tree Topology: Root -> Level 1 -> Leaves
    NetDeviceContainer treeDevices[3];
    treeDevices[0] = p2p.Install(NodeContainer(treeNodes.Get(0), treeNodes.Get(1)));
    treeDevices[1] = p2p.Install(NodeContainer(treeNodes.Get(0), treeNodes.Get(2)));
    treeDevices[2] = p2p.Install(NodeContainer(treeNodes.Get(2), treeNodes.Get(3)));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(meshNodes);
    stack.Install(treeNodes);

    // Assign IP addresses for mesh topology
    Ipv4AddressHelper addressMesh;
    addressMesh.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces[4][4];
    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            meshInterfaces[i][j] = addressMesh.Assign(meshDevices[i][j]);
            addressMesh.NewNetwork();
        }
    }

    // Assign IP addresses for tree topology
    Ipv4AddressHelper addressTree;
    addressTree.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i) {
        treeInterfaces[i] = addressTree.Assign(treeDevices[i]);
        addressTree.NewNetwork();
    }

    // Setup UDP applications
    uint16_t port = 9;

    // Mesh topology traffic: Node 0 sends to Node 3 via multiple paths
    UdpEchoServerHelper meshServer(port);
    ApplicationContainer meshServerApp = meshServer.Install(meshNodes.Get(3));
    meshServerApp.Start(Seconds(1.0));
    meshServerApp.Stop(Seconds(20.0));

    UdpEchoClientHelper meshClient(meshInterfaces[2][3].GetAddress(1), port);
    meshClient.SetAttribute("MaxPackets", UintegerValue(20));
    meshClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    meshClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer meshClientApp = meshClient.Install(meshNodes.Get(0));
    meshClientApp.Start(Seconds(2.0));
    meshClientApp.Stop(Seconds(20.0));

    // Tree topology traffic: Node 0 sends to Node 3 (must go through Node 2)
    UdpEchoServerHelper treeServer(port);
    ApplicationContainer treeServerApp = treeServer.Install(treeNodes.Get(3));
    treeServerApp.Start(Seconds(1.0));
    treeServerApp.Stop(Seconds(20.0));

    UdpEchoClientHelper treeClient(treeInterfaces[2].GetAddress(1), port);
    treeClient.SetAttribute("MaxPackets", UintegerValue(20));
    treeClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    treeClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer treeClientApp = treeClient.Install(treeNodes.Get(0));
    treeClientApp.Start(Seconds(2.0));
    treeClientApp.Stop(Seconds(20.0));

    // Mobility for animation
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(meshNodes);
    mobility.Install(treeNodes);

    // Start movement for animation effect
    for (NodeContainer::Iterator it = meshNodes.Begin(); it != meshNodes.End(); ++it) {
        (*it)->GetObject<MobilityModel>()->SetVelocity(Vector(1.0, 0.0, 0.0));
    }
    for (NodeContainer::Iterator it = treeNodes.Begin(); it != treeNodes.End(); ++it) {
        (*it)->GetObject<MobilityModel>()->SetVelocity(Vector(0.0, 1.0, 0.0));
    }

    // Animation
    AnimationInterface anim("topology-comparison.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routing-table-wireless.xml", Seconds(0), Seconds(20), Seconds(0.25));

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> meshMonitor = flowmon.Install(meshNodes);
    Ptr<FlowMonitor> treeMonitor = flowmon.Install(treeNodes);

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Collect flow statistics
    std::cout << "\n--- Mesh Topology Statistics ---\n";
    meshMonitor->CheckForLostPackets();
    meshMonitor->SerializeToXmlFile("mesh-flow.xml", true, true);
    std::map<FlowId, FlowMonitor::FlowStats> meshStats = meshMonitor->GetFlowStats();
    for (auto &iter : meshStats) {
        std::cout << "Flow ID: " << iter.first << " Packets Dropped: " << iter.second.lostPackets << std::endl;
    }

    std::cout << "\n--- Tree Topology Statistics ---\n";
    treeMonitor->CheckForLostPackets();
    treeMonitor->SerializeToXmlFile("tree-flow.xml", true, true);
    std::map<FlowId, FlowMonitor::FlowStats> treeStats = treeMonitor->GetFlowStats();
    for (auto &iter : treeStats) {
        std::cout << "Flow ID: " << iter.first << " Packets Dropped: " << iter.second.lostPackets << std::endl;
    }

    Simulator::Destroy();
    return 0;
}