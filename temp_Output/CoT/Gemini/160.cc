#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshVsTree");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetDefaultLogComponentEnable(true);

    // Mesh Network
    NodeContainer meshNodes;
    meshNodes.Create(4);

    PointToPointHelper meshPointToPoint;
    meshPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    meshPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer meshDevices[4];
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            NetDeviceContainer link = meshPointToPoint.Install(meshNodes.Get(i), meshNodes.Get(j));
            meshDevices[i].Add(link.Get(0));
            meshDevices[j].Add(link.Get(1));
        }
    }

    // Tree Network
    NodeContainer treeNodes;
    treeNodes.Create(4);

    PointToPointHelper treePointToPoint;
    treePointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    treePointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer treeDevices;
    treeDevices.Add(treePointToPoint.Install(treeNodes.Get(0), treeNodes.Get(1)));
    treeDevices.Add(treePointToPoint.Install(treeNodes.Get(0), treeNodes.Get(2)));
    treeDevices.Add(treePointToPoint.Install(treeNodes.Get(1), treeNodes.Get(3)));

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(meshNodes);
    stack.Install(treeNodes);

    // Assign Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces[4];
    for(int i = 0; i < 4; ++i){
        meshInterfaces[i] = address.Assign(meshDevices[i]);
        address.NewNetwork();
    }

    Ipv4InterfaceContainer treeInterfaces;
    address.SetBase("10.2.1.0", "255.255.255.0");
    treeInterfaces = address.Assign(treeDevices);
    Ipv4InterfaceContainer tempTreeInterfaces;
    tempTreeInterfaces.Add(treeInterfaces.Get(0));
    tempTreeInterfaces.Add(treeInterfaces.Get(1));
    tempTreeInterfaces.Add(treeInterfaces.Get(2));
    tempTreeInterfaces.Add(treeInterfaces.Get(3));
    tempTreeInterfaces.Add(treeInterfaces.Get(4));
    tempTreeInterfaces.Add(treeInterfaces.Get(5));

    // UDP Traffic - Mesh
    UdpEchoServerHelper echoServerMesh(9);
    ApplicationContainer serverAppsMesh = echoServerMesh.Install(meshNodes.Get(3));
    serverAppsMesh.Start(Seconds(1.0));
    serverAppsMesh.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClientMesh(meshInterfaces[3].GetAddress(0), 9);
    echoClientMesh.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClientMesh.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClientMesh.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsMesh = echoClientMesh.Install(meshNodes.Get(0));
    clientAppsMesh.Start(Seconds(2.0));
    clientAppsMesh.Stop(Seconds(20.0));

    // UDP Traffic - Tree
    UdpEchoServerHelper echoServerTree(9);
    ApplicationContainer serverAppsTree = echoServerTree.Install(treeNodes.Get(3));
    serverAppsTree.Start(Seconds(1.0));
    serverAppsTree.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClientTree(tempTreeInterfaces.GetAddress(5), 9);
    echoClientTree.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClientTree.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClientTree.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsTree = echoClientTree.Install(treeNodes.Get(0));
    clientAppsTree.Start(Seconds(2.0));
    clientAppsTree.Stop(Seconds(20.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);
    mobility.Install(treeNodes);

    // Flow Monitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Animation
    AnimationInterface anim("mesh-vs-tree.xml");
    anim.SetConstantPosition(meshNodes.Get(0), 10, 10);
    anim.SetConstantPosition(meshNodes.Get(1), 20, 10);
    anim.SetConstantPosition(meshNodes.Get(2), 10, 20);
    anim.SetConstantPosition(meshNodes.Get(3), 20, 20);

    anim.SetConstantPosition(treeNodes.Get(0), 40, 10);
    anim.SetConstantPosition(treeNodes.Get(1), 50, 10);
    anim.SetConstantPosition(treeNodes.Get(2), 40, 20);
    anim.SetConstantPosition(treeNodes.Get(3), 50, 20);

    // Run Simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Output Flow Monitor Statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalRxBytesMesh = 0;
    double totalRxBytesTree = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Bytes:   " << i->second.txBytes << std::endl;
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << std::endl;
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
        if(t.sourceAddress == Ipv4Address("10.1.1.1"))
            totalRxBytesMesh = i->second.rxBytes;
        if(t.sourceAddress == Ipv4Address("10.2.1.1"))
            totalRxBytesTree = i->second.rxBytes;
    }

    std::cout << "Total Rx Bytes Mesh: " << totalRxBytesMesh << std::endl;
    std::cout << "Total Rx Bytes Tree: " << totalRxBytesTree << std::endl;

    Simulator::Destroy();

    return 0;
}