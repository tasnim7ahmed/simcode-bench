#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>

using namespace ns3;

// A helper function to print FlowMonitor results
void PrintFlowMonitorResults(Ptr<FlowMonitor> flowMonitor, double simulationTime, const std::string& topologyName)
{
    std::cout << "\n--- FlowMonitor Results for " << topologyName << " ---\n";

    flowMonitor->CheckFor = FlowMonitor::PACKETS_RECEIVED | FlowMonitor::BYTES_RECEIVED |
                            FlowMonitor::DELAY_SUM | FlowMonitor::JITTER_SUM;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (FlowMonitorHelper::Get
    Classifier (flowMonitor));
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetStats ();

    if (stats.empty())
    {
        std::cout << "  No flows observed for " << topologyName << ".\n";
        return;
    }

    // Aggregate statistics across all flows for this topology
    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalTxBytes = 0;
    uint64_t totalRxBytes = 0;
    double totalDelaySum = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

        std::cout << "  Flow ID " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "    Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "    Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "    Tx Bytes: " << i->second.txBytes << "\n";
        std::cout << "    Rx Bytes: " << i->second.rxBytes << "\n";

        double throughput = 0.0;
        if (simulationTime > 0)
        {
            throughput = (i->second.rxBytes * 8.0) / (simulationTime * 1024 * 1024); // Mbps
        }
        std::cout << "    Throughput: " << throughput << " Mbps\n";

        double pdr = 0.0;
        if (i->second.txPackets > 0)
        {
            pdr = static_cast<double>(i->second.rxPackets) / i->second.txPackets * 100.0;
        }
        std::cout << "    PDR: " << pdr << " %\n";

        double latency = 0.0;
        if (i->second.rxPackets > 0)
        {
            latency = i->second.delaySum.GetSeconds() / i->second.rxPackets; // Seconds
        }
        std::cout << "    Latency: " << latency * 1000 << " ms\n";

        totalTxPackets += i->second.txPackets;
        totalRxPackets += i->second.rxPackets;
        totalTxBytes += i->second.txBytes;
        totalRxBytes += i->second.rxBytes;
        totalDelaySum += i->second.delaySum.GetSeconds();
    }

    std::cout << "\n  --- Aggregate Statistics for " << topologyName << " ---\n";
    std::cout << "    Total Tx Packets: " << totalTxPackets << "\n";
    std::cout << "    Total Rx Packets: " << totalRxPackets << "\n";
    std::cout << "    Total Tx Bytes: " << totalTxBytes << "\n";
    std::cout << "    Total Rx Bytes: " << totalRxBytes << "\n";

    double aggregateThroughput = 0.0;
    if (simulationTime > 0)
    {
        aggregateThroughput = (totalRxBytes * 8.0) / (simulationTime * 1024 * 1024); // Mbps
    }
    std::cout << "    Aggregate Throughput: " << aggregateThroughput << " Mbps\n";

    double aggregatePdr = 0.0;
    if (totalTxPackets > 0)
    {
        aggregatePdr = static_cast<double>(totalRxPackets) / totalTxPackets * 100.0;
    }
    std::cout << "    Aggregate PDR: " << aggregatePdr << " %\n";

    double aggregateLatency = 0.0;
    if (totalRxPackets > 0)
    {
        aggregateLatency = totalDelaySum / totalRxPackets; // Seconds
    }
    std::cout << "    Aggregate Latency: " << aggregateLatency * 1000 << " ms\n";

    std::cout << "--------------------------------------------------\n";
}


int main(int argc, char *argv[])
{
    // Set default values for simulation parameters
    double simulationTime = 20.0; // seconds
    uint32_t payloadSize = 1024;  // bytes
    std::string dataRate = "10Mbps";
    std::string delay = "10ms";
    uint32_t packetIntervalMs = 100; // milliseconds between packets

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Size of UDP payload in bytes", payloadSize);
    cmd.AddValue("dataRate", "Data rate for P2P links", dataRate);
    cmd.AddValue("delay", "Delay for P2P links", delay);
    cmd.Parse(argc, argv);

    // Configure logging components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_INFO);

    // Configure PointToPointHelper for common link attributes
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2pHelper.SetChannelAttribute("Delay", StringValue(delay));

    // Disable fragmentation and set large queue size to avoid drops due to full queues
    Config::SetDefault("ns3::DropTailQueue::MaxBytes", StringValue("10000000"));

    // -------------------------------------------------------------------------
    // MESH TOPOLOGY SETUP (4 nodes, fully connected using PointToPoint links)
    // Nodes: M0, M1, M2, M3
    // Links: (M0,M1), (M0,M2), (M0,M3), (M1,M2), (M1,M3), (M2,M3) - total 6 links
    // -------------------------------------------------------------------------
    std::cout << "Setting up Mesh Topology...\n";
    NodeContainer meshNodes;
    meshNodes.Create(4); // M0, M1, M2, M3

    InternetStackHelper meshStack;
    meshStack.Install(meshNodes);

    NetDeviceContainer meshDevs[6];
    Ipv4AddressHelper meshAddrHelper;
    
    // Link M0-M1
    meshAddrHelper.SetBase("10.0.0.0", "255.255.255.252");
    meshDevs[0] = p2pHelper.Install(meshNodes.Get(0), meshNodes.Get(1));
    meshAddrHelper.Assign(meshDevs[0]);

    // Link M0-M2
    meshAddrHelper.SetBase("10.0.0.4", "255.255.255.252");
    meshDevs[1] = p2pHelper.Install(meshNodes.Get(0), meshNodes.Get(2));
    meshAddrHelper.Assign(meshDevs[1]);

    // Link M0-M3
    meshAddrHelper.SetBase("10.0.0.8", "255.255.255.252");
    meshDevs[2] = p2pHelper.Install(meshNodes.Get(0), meshNodes.Get(3));
    Ipv4InterfaceContainer meshIf03 = meshAddrHelper.Assign(meshDevs[2]);

    // Link M1-M2
    meshAddrHelper.SetBase("10.0.0.12", "255.255.255.252");
    meshDevs[3] = p2pHelper.Install(meshNodes.Get(1), meshNodes.Get(2));
    meshAddrHelper.Assign(meshDevs[3]);

    // Link M1-M3
    meshAddrHelper.SetBase("10.0.0.16", "255.255.255.252");
    meshDevs[4] = p2pHelper.Install(meshNodes.Get(1), meshNodes.Get(3));
    meshAddrHelper.Assign(meshDevs[4]);

    // Link M2-M3
    meshAddrHelper.SetBase("10.0.0.20", "255.255.255.252");
    meshDevs[5] = p2pHelper.Install(meshNodes.Get(2), meshNodes.Get(3));
    meshAddrHelper.Assign(meshDevs[5]);

    // Setup UDP application for Mesh (M0 -> M3)
    UdpEchoServerHelper meshEchoServer(9);
    ApplicationContainer meshServerApp = meshEchoServer.Install(meshNodes.Get(3)); // M3 as server
    meshServerApp.Start(Seconds(1.0));
    meshServerApp.Stop(Seconds(simulationTime - 1.0));

    // Get the address of M3 to send packets to. Using one of its assigned IP addresses.
    Ipv4Address meshSinkAddress = meshIf03.GetAddress(1); // M3's address on the 10.0.0.8 network

    UdpEchoClientHelper meshEchoClient(meshSinkAddress, 9);
    meshEchoClient.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send as many as possible
    meshEchoClient.SetAttribute("Interval", TimeValue(MilliSeconds(packetIntervalMs)));
    meshEchoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer meshClientApp = meshEchoClient.Install(meshNodes.Get(0)); // M0 as client
    meshClientApp.Start(Seconds(1.5));
    meshClientApp.Stop(Seconds(simulationTime - 0.5));

    // -------------------------------------------------------------------------
    // TREE TOPOLOGY SETUP (4 nodes, T0 root, T1, T2 children of T0, T3 child of T1)
    // T0
    // | \
    // T1 T2
    // |
    // T3
    // -------------------------------------------------------------------------
    std::cout << "Setting up Tree Topology...\n";
    NodeContainer treeNodes;
    treeNodes.Create(4); // T0, T1, T2, T3

    InternetStackHelper treeStack;
    treeStack.Install(treeNodes);

    NetDeviceContainer treeDevs[3];
    Ipv4AddressHelper treeAddrHelper;

    // T0-T1 link
    treeAddrHelper.SetBase("10.1.0.0", "255.255.255.252");
    treeDevs[0] = p2pHelper.Install(treeNodes.Get(0), treeNodes.Get(1));
    treeAddrHelper.Assign(treeDevs[0]);

    // T0-T2 link
    treeAddrHelper.SetBase("10.1.0.4", "255.255.255.252");
    treeDevs[1] = p2pHelper.Install(treeNodes.Get(0), treeNodes.Get(2));
    treeAddrHelper.Assign(treeDevs[1]);

    // T1-T3 link
    treeAddrHelper.SetBase("10.1.0.8", "255.255.255.252");
    treeDevs[2] = p2pHelper.Install(treeNodes.Get(1), treeNodes.Get(3));
    Ipv4InterfaceContainer treeIf13 = treeAddrHelper.Assign(treeDevs[2]);

    // Populate routing tables for both topologies
    // This must be done after all IP addresses are assigned.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP application for Tree (T0 -> T3)
    UdpEchoServerHelper treeEchoServer(9);
    ApplicationContainer treeServerApp = treeEchoServer.Install(treeNodes.Get(3)); // T3 as server
    treeServerApp.Start(Seconds(1.0));
    treeServerApp.Stop(Seconds(simulationTime - 1.0));

    // Get the address of T3 to send packets to.
    Ipv4Address treeSinkAddress = treeIf13.GetAddress(1); // T3's address on the 10.1.0.8 network

    UdpEchoClientHelper treeEchoClient(treeSinkAddress, 9);
    treeEchoClient.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    treeEchoClient.SetAttribute("Interval", TimeValue(MilliSeconds(packetIntervalMs)));
    treeEchoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer treeClientApp = treeEchoClient.Install(treeNodes.Get(0)); // T0 as client
    treeClientApp.Start(Seconds(1.5));
    treeClientApp.Stop(Seconds(simulationTime - 0.5));

    // -------------------------------------------------------------------------
    // NetAnim Visualization Setup
    // -------------------------------------------------------------------------
    std::cout << "Setting up NetAnim visualization...\n";
    NetAnimHelper netanim;
    netanim.SetOutputFileName("mesh-vs-tree.xml");

    // Set positions for Mesh nodes for better visualization
    Ptr<ListPositionAllocator> meshPositionAlloc = CreateObject<ListPositionAllocator>();
    meshPositionAlloc->Add(Vector(0.0, 0.0, 0.0));   // M0
    meshPositionAlloc->Add(Vector(50.0, 0.0, 0.0));  // M1
    meshPositionAlloc->Add(Vector(0.0, 50.0, 0.0));  // M2
    meshPositionAlloc->Add(Vector(50.0, 50.0, 0.0)); // M3
    MobilityHelper meshMobility;
    meshMobility.SetPositionAllocator(meshPositionAlloc);
    meshMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    meshMobility.Install(meshNodes);

    // Set positions for Tree nodes for better visualization (offset to avoid overlap)
    Ptr<ListPositionAllocator> treePositionAlloc = CreateObject<ListPositionAllocator>();
    treePositionAlloc->Add(Vector(150.0, 0.0, 0.0));  // T0 (root)
    treePositionAlloc->Add(Vector(125.0, 50.0, 0.0)); // T1
    treePositionAlloc->Add(Vector(175.0, 50.0, 0.0)); // T2
    treePositionAlloc->Add(Vector(125.0, 100.0, 0.0)); // T3
    MobilityHelper treeMobility;
    treeMobility.SetPositionAllocator(treePositionAlloc);
    treeMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    treeMobility.Install(treeNodes);

    netanim.EnablePacketTracking();
    netanim.EnableAnimation(); // Ensure animation is enabled

    // -------------------------------------------------------------------------
    // FlowMonitor Setup
    // -------------------------------------------------------------------------
    std::cout << "Setting up FlowMonitor...\n";
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> meshFlowMonitor = flowMonitorHelper.Install(meshNodes);
    Ptr<FlowMonitor> treeFlowMonitor = flowMonitorHelper.Install(treeNodes);

    // -------------------------------------------------------------------------
    // Simulation Run
    // -------------------------------------------------------------------------
    std::cout << "Starting simulation for " << simulationTime << " seconds...\n";
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    std::cout << "Simulation finished.\n";

    // -------------------------------------------------------------------------
    // Collect and Print Results
    // -------------------------------------------------------------------------
    PrintFlowMonitorResults(meshFlowMonitor, simulationTime, "Mesh Topology");
    PrintFlowMonitorResults(treeFlowMonitor, simulationTime, "Tree Topology");

    // Clean up
    meshFlowMonitor->Stop();
    meshFlowMonitor->Dispose();
    treeFlowMonitor->Stop();
    treeFlowMonitor->Dispose();
    Simulator::Destroy();

    return 0;
}