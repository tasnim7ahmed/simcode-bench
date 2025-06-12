#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TopologyComparisonSimulation");

void PrintPerformanceMetrics(Ptr<FlowMonitor> flowMonitor, const std::string& topologyType)
{
    FlowMonitorHelper::SerializeStatsHeader(std::cout);
    for (auto flow : flowMonitor->GetFlows())
    {
        FlowMonitorHelper::SerializeObjectSizeStats(std::cout, flow.first, flow.second);
    }

    std::cout << "\nPerformance Metrics for " << topologyType << " Topology:\n";
    for (auto flow : flowMonitor->GetFlows())
    {
        std::cout << "Flow ID: " << flow.first << " | ";
        std::cout << "Packets Dropped: " << flow.second.packetsDropped.size() << " | ";
        std::cout << "Packets Delivered: " << flow.second.rxPackets << " | ";
        std::cout << "Throughput: " << ((flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds())) / 1000 << " Kbps | ";
        std::cout << "Average Delay: " << flow.second.delaySum.GetSeconds() / flow.second.rxPackets << " s\n";
    }
}

int main(int argc, char *argv[])
{
    bool enableNetAnim = true;
    uint32_t runSeed = 1;
    double simulationTime = 20.0;

    RngSeedManager::SetSeed(runSeed);

    // Mesh Topology Setup
    NodeContainer meshNodes;
    meshNodes.Create(4);

    PointToPointHelper meshPointToPoint;
    meshPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    meshPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer meshDevices[6];
    meshDevices[0] = meshPointToPoint.Install(meshNodes.Get(0), meshNodes.Get(1));
    meshDevices[1] = meshPointToPoint.Install(meshNodes.Get(0), meshNodes.Get(2));
    meshDevices[2] = meshPointToPoint.Install(meshNodes.Get(0), meshNodes.Get(3));
    meshDevices[3] = meshPointToPoint.Install(meshNodes.Get(1), meshNodes.Get(2));
    meshDevices[4] = meshPointToPoint.Install(meshNodes.Get(1), meshNodes.Get(3));
    meshDevices[5] = meshPointToPoint.Install(meshNodes.Get(2), meshNodes.Get(3));

    InternetStackHelper stack;
    stack.Install(meshNodes);

    Ipv4AddressHelper meshIpv4;
    Ipv4InterfaceContainer meshInterfaces[6];
    for (uint32_t i = 0; i < 6; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        meshIpv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        meshInterfaces[i] = meshIpv4.Assign(meshDevices[i]);
    }

    // Tree Topology Setup
    NodeContainer treeNodes;
    treeNodes.Create(4);

    NodeContainer treeRoot = treeNodes.Get(0);
    NodeContainer treeLevel1A = treeNodes.Get(1);
    NodeContainer treeLevel1B = treeNodes.Get(2);
    NodeContainer treeLeaf = treeNodes.Get(3);

    PointToPointHelper treePointToPoint;
    treePointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    treePointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer treeDevices1 = treePointToPoint.Install(treeRoot, treeLevel1A);
    NetDeviceContainer treeDevices2 = treePointToPoint.Install(treeRoot, treeLevel1B);
    NetDeviceContainer treeDevices3 = treePointToPoint.Install(treeLevel1A, treeLeaf);

    stack.Install(treeNodes);

    Ipv4AddressHelper treeIpv4;
    Ipv4InterfaceContainer treeInterfaces1, treeInterfaces2, treeInterfaces3;
    treeIpv4.SetBase("10.2.1.0", "255.255.255.0");
    treeInterfaces1 = treeIpv4.Assign(treeDevices1);
    treeIpv4.SetBase("10.2.2.0", "255.255.255.0");
    treeInterfaces2 = treeIpv4.Assign(treeDevices2);
    treeIpv4.SetBase("10.2.3.0", "255.255.255.0");
    treeInterfaces3 = treeIpv4.Assign(treeDevices3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Applications
    uint16_t port = 9;

    // Mesh traffic
    OnOffHelper meshOnOff("ns3::UdpSocketFactory", Address(InetSocketAddress(meshInterfaces[1].GetAddress(1), port)));
    meshOnOff.SetConstantRate(DataRate("1Mbps"));
    ApplicationContainer meshApps = meshOnOff.Install(meshNodes.Get(0));
    meshApps.Start(Seconds(1.0));
    meshApps.Stop(Seconds(simulationTime));

    PacketSinkHelper meshSink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer meshSinkApp = meshSink.Install(meshNodes.Get(2));
    meshSinkApp.Start(Seconds(1.0));
    meshSinkApp.Stop(Seconds(simulationTime));

    // Tree traffic
    OnOffHelper treeOnOff("ns3::UdpSocketFactory", Address(InetSocketAddress(treeInterfaces3.GetAddress(1), port)));
    treeOnOff.SetConstantRate(DataRate("1Mbps"));
    ApplicationContainer treeApps = treeOnOff.Install(treeRoot);
    treeApps.Start(Seconds(1.0));
    treeApps.Stop(Seconds(simulationTime));

    PacketSinkHelper treeSink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer treeSinkApp = treeSink.Install(treeLeaf);
    treeSinkApp.Start(Seconds(1.0));
    treeSinkApp.Stop(Seconds(simulationTime));

    // Mobility for animation
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);
    mobility.Install(treeNodes);

    // NetAnim Animation
    if (enableNetAnim)
    {
        AnimationInterface anim("topology-comparison.xml");
        anim.EnablePacketMetadata(true);
        anim.EnableIpv4RouteTracking("routing-tables.xml", Seconds(0), Seconds(simulationTime), Seconds(0.25));
    }

    // Flow Monitor
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> meshFlowMonitor = flowMonitorHelper.Install(meshNodes);
    Ptr<FlowMonitor> treeFlowMonitor = flowMonitorHelper.Install(treeNodes);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    std::cout << "\n--- Mesh Topology Performance ---" << std::endl;
    PrintPerformanceMetrics(meshFlowMonitor, "Mesh");

    std::cout << "\n--- Tree Topology Performance ---" << std::endl;
    PrintPerformanceMetrics(treeFlowMonitor, "Tree");

    Simulator::Destroy();
    return 0;
}