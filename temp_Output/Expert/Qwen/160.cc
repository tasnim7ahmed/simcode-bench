#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshVsTreeTopologyComparison");

void CreateMesh(Ptr<Node> n0, Ptr<Node> n1, Ptr<Node> n2, Ptr<Node> n3, NetDeviceContainer &devices, Ipv4InterfaceContainer &interfaces)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(n0, n1);
    NetDeviceContainer d02 = p2p.Install(n0, n2);
    NetDeviceContainer d03 = p2p.Install(n0, n3);
    NetDeviceContainer d12 = p2p.Install(n1, n2);
    NetDeviceContainer d13 = p2p.Install(n1, n3);
    NetDeviceContainer d23 = p2p.Install(n2, n3);

    devices.Add(d01);
    devices.Add(d02);
    devices.Add(d03);
    devices.Add(d12);
    devices.Add(d13);
    devices.Add(d23);

    InternetStackHelper stack;
    stack.Install(NodeContainer(n0, n1, n2, n3));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(d01));
    address.NewNetwork();
    interfaces.Add(address.Assign(d02));
    address.NewNetwork();
    interfaces.Add(address.Assign(d03));
    address.NewNetwork();
    interfaces.Add(address.Assign(d12));
    address.NewNetwork();
    interfaces.Add(address.Assign(d13));
    address.NewNetwork();
    interfaces.Add(address.Assign(d23));
}

void CreateTree(Ptr<Node> root, Ptr<Node> child1, Ptr<Node> child2, Ptr<Node> child11, NetDeviceContainer &devices, Ipv4InterfaceContainer &interfaces)
{
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dRootChild1 = p2p.Install(root, child1);
    NetDeviceContainer dRootChild2 = p2p.Install(root, child2);
    NetDeviceContainer dChild1Child11 = p2p.Install(child1, child11);

    devices.Add(dRootChild1);
    devices.Add(dRootChild2);
    devices.Add(dChild1Child11);

    InternetStackHelper stack;
    stack.Install(NodeContainer(root, child1, child2, child11));

    Ipv4AddressHelper address;
    address.SetBase("10.2.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(dRootChild1));
    address.NewNetwork();
    interfaces.Add(address.Assign(dRootChild2));
    address.NewNetwork();
    interfaces.Add(address.Assign(dChild1Child11));
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer treeNodes;
    treeNodes.Create(4);

    NetDeviceContainer meshDevices;
    Ipv4InterfaceContainer meshInterfaces;
    CreateMesh(meshNodes.Get(0), meshNodes.Get(1), meshNodes.Get(2), meshNodes.Get(3), meshDevices, meshInterfaces);

    NetDeviceContainer treeDevices;
    Ipv4InterfaceContainer treeInterfaces;
    CreateTree(treeNodes.Get(0), treeNodes.Get(1), treeNodes.Get(2), treeNodes.Get(3), treeDevices, treeInterfaces);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer meshServerApps = echoServer.Install(meshNodes.Get(3));
    meshServerApps.Start(Seconds(1.0));
    meshServerApps.Stop(Seconds(20.0));

    UdpEchoClientHelper meshEchoClient(meshInterfaces.GetAddress(5, 1), 9);
    meshEchoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    meshEchoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    meshEchoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer meshClientApps = meshEchoClient.Install(meshNodes.Get(0));
    meshClientApps.Start(Seconds(2.0));
    meshClientApps.Stop(Seconds(20.0));

    UdpEchoServerHelper treeEchoServer(9);
    ApplicationContainer treeServerApps = treeEchoServer.Install(treeNodes.Get(3));
    treeServerApps.Start(Seconds(1.0));
    treeServerApps.Stop(Seconds(20.0));

    UdpEchoClientHelper treeEchoClient(treeInterfaces.GetAddress(2, 1), 9);
    treeEchoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    treeEchoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    treeEchoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer treeClientApps = treeEchoClient.Install(treeNodes.Get(0));
    treeClientApps.Start(Seconds(2.0));
    treeClientApps.Stop(Seconds(20.0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(NodeContainer::GetGlobal());

    AnimationInterface anim("mesh-vs-tree.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routing-table-wireless.xml", Seconds(0), Seconds(5), Seconds(0.25));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> meshMonitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20));
    Simulator::Run();

    std::cout << "\n--- Mesh Topology Metrics ---" << std::endl;
    meshMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> meshClassifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> meshStats = meshMonitor->GetFlowStats();
    for (auto &[id, stats] : meshStats)
    {
        Ipv4FlowClassifier::FiveTuple t = meshClassifier->FindFlow(id);
        if (t.sourcePort == 9 || t.destinationPort == 9)
        {
            std::cout << "Flow ID: " << id << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "  Tx Packets: " << stats.txPackets << std::endl;
            std::cout << "  Rx Packets: " << stats.rxPackets << std::endl;
            std::cout << "  Throughput: " << stats.rxBytes * 8.0 / (stats.timeLastRxPacket.GetSeconds() - stats.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
            std::cout << "  Mean Delay: " << stats.delaySum.GetSeconds() / stats.rxPackets << " s" << std::endl;
            std::cout << "  Packet Delivery Ratio: " << static_cast<double>(stats.rxPackets) / stats.txPackets << std::endl;
        }
    }

    std::cout << "\n--- Tree Topology Metrics ---" << std::endl;
    Ptr<FlowMonitor> treeMonitor = flowmon.InstallAll();
    treeMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> treeClassifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> treeStats = treeMonitor->GetFlowStats();
    for (auto &[id, stats] : treeStats)
    {
        Ipv4FlowClassifier::FiveTuple t = treeClassifier->FindFlow(id);
        if (t.sourcePort == 9 || t.destinationPort == 9)
        {
            std::cout << "Flow ID: " << id << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "  Tx Packets: " << stats.txPackets << std::endl;
            std::cout << "  Rx Packets: " << stats.rxPackets << std::endl;
            std::cout << "  Throughput: " << stats.rxBytes * 8.0 / (stats.timeLastRxPacket.GetSeconds() - stats.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
            std::cout << "  Mean Delay: " << stats.delaySum.GetSeconds() / stats.rxPackets << " s" << std::endl;
            std::cout << "  Packet Delivery Ratio: " << static_cast<double>(stats.rxPackets) / stats.txPackets << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}