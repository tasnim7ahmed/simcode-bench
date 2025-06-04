#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshVsTreeComparison");

int main(int argc, char *argv[])
{
    // Simulation time
    double simTime = 20.0;

    // Create nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer treeNodes;
    treeNodes.Create(4);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(meshNodes);
    internet.Install(treeNodes);

    // Configure mesh topology
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

    // Configure tree topology
    Ipv4AddressHelper addressTree;
    addressTree.SetBase("10.2.2.0", "255.255.255.0");

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer treeDevices1 = p2p.Install(treeNodes.Get(0), treeNodes.Get(1));
    NetDeviceContainer treeDevices2 = p2p.Install(treeNodes.Get(0), treeNodes.Get(2));
    NetDeviceContainer treeDevices3 = p2p.Install(treeNodes.Get(1), treeNodes.Get(3));

    Ipv4InterfaceContainer treeInterfaces1 = addressTree.Assign(treeDevices1);
    Ipv4InterfaceContainer treeInterfaces2 = addressTree.Assign(treeDevices2);
    Ipv4InterfaceContainer treeInterfaces3 = addressTree.Assign(treeDevices3);

    // Set mobility models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);
    mobility.Install(treeNodes);

    // UDP traffic configuration
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverAppsMesh = udpServer.Install(meshNodes.Get(0));
    serverAppsMesh.Start(Seconds(1.0));
    serverAppsMesh.Stop(Seconds(simTime));

    UdpServerHelper udpServerTree(port);
    ApplicationContainer serverAppsTree = udpServerTree.Install(treeNodes.Get(0));
    serverAppsTree.Start(Seconds(1.0));
    serverAppsTree.Stop(Seconds(simTime));

    UdpClientHelper udpClientMesh(meshInterfaces.GetAddress(0), port);
    udpClientMesh.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClientMesh.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClientMesh.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsMesh = udpClientMesh.Install(meshNodes.Get(1));
    clientAppsMesh.Start(Seconds(2.0));
    clientAppsMesh.Stop(Seconds(simTime));

    UdpClientHelper udpClientTree(treeInterfaces1.GetAddress(0), port);
    udpClientTree.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClientTree.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClientTree.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsTree = udpClientTree.Install(treeNodes.Get(1));
    clientAppsTree.Start(Seconds(2.0));
    clientAppsTree.Stop(Seconds(simTime));

    // Set up FlowMonitor to gather statistics
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // NetAnim setup
    AnimationInterface anim("mesh_vs_tree.xml");
    anim.SetConstantPosition(meshNodes.Get(0), 10, 20);
    anim.SetConstantPosition(meshNodes.Get(1), 20, 30);
    anim.SetConstantPosition(meshNodes.Get(2), 30, 40);
    anim.SetConstantPosition(meshNodes.Get(3), 40, 50);

    anim.SetConstantPosition(treeNodes.Get(0), 50, 20);
    anim.SetConstantPosition(treeNodes.Get(1), 60, 30);
    anim.SetConstantPosition(treeNodes.Get(2), 70, 40);
    anim.SetConstantPosition(treeNodes.Get(3), 80, 50);

    // Run the simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow monitor statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / simTime / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Packet Delivery Ratio: " << (double)it->second.rxPackets / it->second.txPackets * 100 << "%" << std::endl;
    }

    // Clean up
    Simulator::Destroy();

    return 0;
}

