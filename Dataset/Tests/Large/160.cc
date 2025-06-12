#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshVsTreeComparisonTest");

// Test 1: Verify Node Creation for Mesh and Tree Topologies
void TestNodeCreation()
{
    NodeContainer meshNodes;
    meshNodes.Create(4);  // Create 4 nodes for the mesh topology

    NodeContainer treeNodes;
    treeNodes.Create(4);  // Create 4 nodes for the tree topology

    // Ensure correct number of nodes are created
    NS_ASSERT_MSG(meshNodes.GetN() == 4, "Mesh topology should have 4 nodes.");
    NS_ASSERT_MSG(treeNodes.GetN() == 4, "Tree topology should have 4 nodes.");
}

// Test 2: Verify Mesh Topology Installation and Configuration
void TestMeshTopologyConfiguration()
{
    NodeContainer meshNodes;
    meshNodes.Create(4);

    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetStandard(WIFI_PHY_STANDARD_80211s);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    MeshPointDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);
    Ipv4AddressHelper addressMesh;
    addressMesh.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = addressMesh.Assign(meshDevices);

    // Ensure the mesh topology devices and interfaces are installed correctly
    NS_ASSERT_MSG(meshInterfaces.GetN() == 4, "Mesh topology should have 4 interfaces.");
}

// Test 3: Verify Tree Topology Installation and Configuration
void TestTreeTopologyConfiguration()
{
    NodeContainer treeNodes;
    treeNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer treeDevices1 = p2p.Install(treeNodes.Get(0), treeNodes.Get(1));
    NetDeviceContainer treeDevices2 = p2p.Install(treeNodes.Get(0), treeNodes.Get(2));
    NetDeviceContainer treeDevices3 = p2p.Install(treeNodes.Get(1), treeNodes.Get(3));

    Ipv4AddressHelper addressTree;
    addressTree.SetBase("10.2.2.0", "255.255.255.0");

    Ipv4InterfaceContainer treeInterfaces1 = addressTree.Assign(treeDevices1);
    Ipv4InterfaceContainer treeInterfaces2 = addressTree.Assign(treeDevices2);
    Ipv4InterfaceContainer treeInterfaces3 = addressTree.Assign(treeDevices3);

    // Ensure the tree topology devices and interfaces are installed correctly
    NS_ASSERT_MSG(treeInterfaces1.GetN() == 2, "Tree topology should have 2 interfaces for the first link.");
    NS_ASSERT_MSG(treeInterfaces2.GetN() == 2, "Tree topology should have 2 interfaces for the second link.");
    NS_ASSERT_MSG(treeInterfaces3.GetN() == 2, "Tree topology should have 2 interfaces for the third link.");
}

// Test 4: Verify Mobility Model Installation
void TestMobilityModel()
{
    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer treeNodes;
    treeNodes.Create(4);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);
    mobility.Install(treeNodes);

    // Ensure mobility models are installed for both topologies
    NS_ASSERT_MSG(meshNodes.Get(0)->GetObject<MobilityModel>() != nullptr, "Mobility model should be installed on mesh nodes.");
    NS_ASSERT_MSG(treeNodes.Get(0)->GetObject<MobilityModel>() != nullptr, "Mobility model should be installed on tree nodes.");
}

// Test 5: Verify UDP Server Installation for Mesh Nodes
void TestUdpServerMesh()
{
    NodeContainer meshNodes;
    meshNodes.Create(4);

    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverAppsMesh = udpServer.Install(meshNodes.Get(0));
    serverAppsMesh.Start(Seconds(1.0));
    serverAppsMesh.Stop(Seconds(20.0));

    // Ensure the UDP server is installed on the mesh central node
    NS_ASSERT_MSG(serverAppsMesh.GetN() == 1, "UDP Server should be installed on mesh central node.");
}

// Test 6: Verify UDP Server Installation for Tree Nodes
void TestUdpServerTree()
{
    NodeContainer treeNodes;
    treeNodes.Create(4);

    uint16_t port = 9;
    UdpServerHelper udpServerTree(port);
    ApplicationContainer serverAppsTree = udpServerTree.Install(treeNodes.Get(0));
    serverAppsTree.Start(Seconds(1.0));
    serverAppsTree.Stop(Seconds(20.0));

    // Ensure the UDP server is installed on the tree central node
    NS_ASSERT_MSG(serverAppsTree.GetN() == 1, "UDP Server should be installed on tree central node.");
}

// Test 7: Verify UDP Client Installation for Mesh Nodes
void TestUdpClientMesh()
{
    NodeContainer meshNodes;
    meshNodes.Create(4);

    Ipv4InterfaceContainer meshInterfaces;
    Ipv4AddressHelper addressMesh;
    addressMesh.SetBase("10.1.1.0", "255.255.255.0");
    meshInterfaces = addressMesh.Assign(MeshHelper().Install(YansWifiPhyHelper::Default(), meshNodes));

    uint16_t port = 9;
    UdpClientHelper udpClientMesh(meshInterfaces.GetAddress(0), port);
    udpClientMesh.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClientMesh.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClientMesh.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsMesh = udpClientMesh.Install(meshNodes.Get(1));
    clientAppsMesh.Start(Seconds(2.0));
    clientAppsMesh.Stop(Seconds(20.0));

    // Ensure the UDP client is installed on a mesh node
    NS_ASSERT_MSG(clientAppsMesh.GetN() == 1, "UDP Client should be installed on a mesh node.");
}

// Test 8: Verify UDP Client Installation for Tree Nodes
void TestUdpClientTree()
{
    NodeContainer treeNodes;
    treeNodes.Create(4);

    Ipv4InterfaceContainer treeInterfaces1;
    Ipv4AddressHelper addressTree;
    addressTree.SetBase("10.2.2.0", "255.255.255.0");
    treeInterfaces1 = addressTree.Assign(PointToPointHelper().Install(treeNodes.Get(0), treeNodes.Get(1)));

    uint16_t port = 9;
    UdpClientHelper udpClientTree(treeInterfaces1.GetAddress(0), port);
    udpClientTree.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClientTree.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClientTree.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsTree = udpClientTree.Install(treeNodes.Get(1));
    clientAppsTree.Start(Seconds(2.0));
    clientAppsTree.Stop(Seconds(20.0));

    // Ensure the UDP client is installed on a tree node
    NS_ASSERT_MSG(clientAppsTree.GetN() == 1, "UDP Client should be installed on a tree node.");
}

// Test 9: Verify FlowMonitor Installation
void TestFlowMonitorInstallation()
{
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Ensure flow monitor is installed
    NS_ASSERT_MSG(monitor != nullptr, "Flow monitor should be installed.");
}

// Test 10: Verify Flow Monitor Statistics
void TestFlowMonitorStats()
{
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    // Ensure flow statistics are available
    NS_ASSERT_MSG(stats.size() > 0, "Flow statistics should not be empty.");
    Simulator::Destroy();
}

// Test 11: Verify NetAnim Output
void TestNetAnimOutput()
{
    AnimationInterface anim("mesh_vs_tree.xml");

    // Check if the NetAnim file is created
    std::ifstream inFile("mesh_vs_tree.xml");
    NS_ASSERT_MSG(inFile.is_open(), "NetAnim output file should be created and openable.");
    inFile.close();
}

int main (int argc, char *argv[])
{
    // Run individual tests
    TestNodeCreation();
    TestMeshTopologyConfiguration();
    TestTreeTopologyConfiguration();
    TestMobilityModel();
    TestUdpServerMesh();
    TestUdpServerTree();
    TestUdpClientMesh();
    TestUdpClientTree();
    TestFlowMonitorInstallation();
    TestFlowMonitorStats();
    TestNetAnimOutput();

    return 0;
}
