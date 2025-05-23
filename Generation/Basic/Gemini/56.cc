#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/red-queue-disc.h" // Use RedQueueDisc for TrafficControl queues
#include "ns3/tcp-dctcp.h"
#include "ns3/traffic-control-module.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cmath> // For pow, sqrt, etc.
#include <vector>
#include <numeric> // For std::accumulate
#include <random> // For random start times

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpTopologySimulation");

// Global counters for queue statistics
uint32_t g_t1T2MarkedPackets = 0;
uint32_t g_t1T2DroppedPackets = 0;
uint32_t g_t2R1MarkedPackets = 0;
uint32_t g_t2R1DroppedPackets = 0;

// Callbacks for queue statistics (connected to QueueDisc trace sources)
void T1T2QueueMark(Ptr<const QueueDiscItem> item)
{
    g_t1T2MarkedPackets++;
}

void T1T2QueueDrop(Ptr<const QueueDiscItem> item)
{
    g_t1T2DroppedPackets++;
}

void T2R1QueueMark(Ptr<const QueueDiscItem> item)
{
    g_t2R1MarkedPackets++;
}

void T2R1QueueDrop(Ptr<const QueueDiscItem> item)
{
    g_t2R1DroppedPackets++;
}


int main(int argc, char *argv[])
{
    // 1. Simulation Parameters
    Time::SetResolution(Time::NS);
    LogComponentEnable("DctcpTopologySimulation", LOG_LEVEL_INFO);
    // Uncomment these for more detailed logging:
    // LogComponentEnable("TcpDctcp", LOG_LEVEL_INFO);
    // LogComponentEnable("RedQueueDisc", LOG_LEVEL_INFO);
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    // LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);

    // Simulation time constants
    const double STARTUP_WINDOW = 1.0;  // Flows staggered within 1 second
    const double CONVERGENCE_TIME = 3.0; // After startup window
    const double MEASUREMENT_INTERVAL = 1.0; // Duration for throughput measurement
    const double SIM_START_TIME = 0.0;
    const double MEASUREMENT_START_TIME = SIM_START_TIME + STARTUP_WINDOW + CONVERGENCE_TIME; // 4.0 seconds
    const double SIM_END_TIME = MEASUREMENT_START_TIME + MEASUREMENT_INTERVAL + 1.0; // 6.0 seconds total

    // Topology parameters
    const uint32_t N_FLOWS_B1 = 30; // Flows bottlenecked by T1-T2
    const uint32_t N_FLOWS_B2 = 20; // Flows bottlenecked by T2-R1
    const uint32_t N_TOTAL_FLOWS = N_FLOWS_B1 + N_FLOWS_B2; // Total 50 TCP flows

    // Link parameters
    const DataRate S_TO_T_RATE("10Gbps"); // Sender to switch links
    const std::string S_TO_T_DELAY("10us");

    const DataRate T1_T2_RATE("10Gbps"); // Bottleneck 1
    const std::string T1_T2_DELAY("10us");

    const DataRate T2_R1_RATE("1Gbps"); // Bottleneck 2
    const std::string T2_R1_DELAY("10us");

    const DataRate R1_TO_D_RATE("10Gbps"); // Router to receiver links
    const std::string R1_TO_D_DELAY("10us");

    // RED Queue parameters (based on DCTCP paper/typical values)
    const uint32_t PACKET_SIZE_BYTES = 1448; // Standard TCP MSS for 1500 byte MTU

    // T1-T2 link (10 Gbps) RED queue: 500 packets buffer, min=5, max=15 packets thresholds
    const uint32_t RED_Q_MAX_BYTES_T1_T2 = 500 * PACKET_SIZE_BYTES; 
    const uint32_t RED_MIN_THRESH_T1_T2 = 5 * PACKET_SIZE_BYTES;
    const uint32_t RED_MAX_THRESH_T1_T2 = 15 * PACKET_SIZE_BYTES;
    const double RED_QW_T1_T2 = 1.0 / 10.0; // K_weight = 10 from DCTCP paper for averaging

    // T2-R1 link (1 Gbps) RED queue: 50 packets buffer, min=5, max=15 packets thresholds
    const uint32_t RED_Q_MAX_BYTES_T2_R1 = 50 * PACKET_SIZE_BYTES; 
    const uint32_t RED_MIN_THRESH_T2_R1 = 5 * PACKET_SIZE_BYTES;
    const uint32_t RED_MAX_THRESH_T2_R1 = 15 * PACKET_SIZE_BYTES;
    const double RED_QW_T2_R1 = 1.0 / 10.0; // K_weight = 10 from DCTCP paper

    // Configure TCP variant (DCTCP)
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpDctcp::GetTypeId()));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(131072)); // 128KB Send Buffer
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(131072)); // 128KB Receive Buffer

    // Configure RED queue attributes for TrafficControlHelper
    Config::SetDefault("ns3::RedQueueDisc::Mode", StringValue("Bytes"));
    Config::SetDefault("ns3::RedQueueDisc::PacketMarking", BooleanValue(true)); // Enable ECN marking
    Config::SetDefault("ns3::RedQueueDisc::EcnsEnabled", BooleanValue(true)); // Explicitly enable ECN
    Config::SetDefault("ns3::RedQueueDisc::DctcpEcn", BooleanValue(true)); // DCTCP-specific ECN behavior
    Config::SetDefault("ns3::RedQueueDisc::UsePerLinkCapacity", BooleanValue(true)); // Allows rate-aware threshold scaling
    Config::SetDefault("ns3::RedQueueDisc::MaxP", DoubleValue(1.0)); // Probability of marking reaches 1.0 at MaxThresh

    // Randomization for flow start times within the startup window
    SeedManager::SetSeed(112233);
    Ptr<UniformRandomVariable> flowStartTimeRng = CreateObject<UniformRandomVariable>();
    flowStartTimeRng->SetAttribute("Min", DoubleValue(0.0));
    flowStartTimeRng->SetAttribute("Max", DoubleValue(STARTUP_WINDOW));

    // 2. Node Creation
    NodeContainer sendersB1Nodes; sendersB1Nodes.Create(N_FLOWS_B1);
    NodeContainer sendersB2Nodes; sendersB2Nodes.Create(N_FLOWS_B2);
    NodeContainer t1Node; t1Node.Create(1);
    NodeContainer t2Node; t2Node.Create(1);
    NodeContainer r1Node; r1Node.Create(1);
    NodeContainer receiverNodes; receiverNodes.Create(N_TOTAL_FLOWS);

    // 3. Link Creation, Device Installation, and QueueDisc Configuration
    PointToPointHelper p2pHelper;
    Ipv4AddressHelper ipv4;
    TrafficControlHelper tcHelper;

    // SendersB1 to T1 links (high-speed access links)
    ipv4.SetBase("10.1.0.0", "255.255.0.0"); // Subnet for S-T1 connections
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(S_TO_T_RATE));
    p2pHelper.SetChannelAttribute("Delay", StringValue(S_TO_T_DELAY));
    tcHelper.SetRootQueueDisc("ns3::NoopQueueDisc"); // No QueueDisc for access links (equivalent to DropTail if not set)
    for (uint32_t i = 0; i < N_FLOWS_B1; ++i)
    {
        NetDeviceContainer devices = p2pHelper.Install(sendersB1Nodes.Get(i), t1Node.Get(0));
        ipv4.Assign(devices);
        tcHelper.Install(devices.Get(0)); // Sender side
        tcHelper.Install(devices.Get(1)); // T1 side
    }

    // SendersB2 to T2 links (high-speed access links)
    ipv4.SetBase("10.2.0.0", "255.255.0.0"); // Subnet for S-T2 connections
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(S_TO_T_RATE));
    p2pHelper.SetChannelAttribute("Delay", StringValue(S_TO_T_DELAY));
    tcHelper.SetRootQueueDisc("ns3::NoopQueueDisc");
    for (uint32_t i = 0; i < N_FLOWS_B2; ++i)
    {
        NetDeviceContainer devices = p2pHelper.Install(sendersB2Nodes.Get(i), t2Node.Get(0));
        ipv4.Assign(devices);
        tcHelper.Install(devices.Get(0)); // Sender side
        tcHelper.Install(devices.Get(1)); // T2 side
    }

    // T1-T2 bottleneck link (10 Gbps with RED/ECN)
    NetDeviceContainer t1T2Devices;
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(T1_T2_RATE));
    p2pHelper.SetChannelAttribute("Delay", StringValue(T1_T2_DELAY));
    t1T2Devices = p2pHelper.Install(t1Node.Get(0), t2Node.Get(0));
    ipv4.SetBase("10.3.1.0", "255.255.255.252");
    ipv4.Assign(t1T2Devices);

    // Install RED QueueDisc on T1's outgoing interface (T1->T2)
    tcHelper.SetRootQueueDisc("ns3::RedQueueDisc",
                                "MaxBytes", UintegerValue(RED_Q_MAX_BYTES_T1_T2),
                                "MinThresh", DoubleValue(RED_MIN_THRESH_T1_T2),
                                "MaxThresh", DoubleValue(RED_MAX_THRESH_T1_T2),
                                "QW", DoubleValue(RED_QW_T1_T2));
    tcHelper.Install(t1T2Devices.Get(0)); // Install on the T1 side of the link (output queue)

    // Get pointer to the RedQueueDisc for tracing
    Ptr<QueueDisc> queueDiscT1T2 = DynamicCast<PointToPointNetDevice>(t1T2Devices.Get(0))->GetQueueDisc();
    NS_ASSERT_MSG(queueDiscT1T2 != nullptr, "QueueDisc not found on T1->T2 device");
    Ptr<RedQueueDisc> redQueueDiscT1T2 = DynamicCast<RedQueueDisc>(queueDiscT1T2);
    NS_ASSERT_MSG(redQueueDiscT1T2 != nullptr, "RedQueueDisc not found on T1->T2 device");
    redQueueDiscT1T2->TraceConnectWithoutContext("Mark", MakeCallback(&T1T2QueueMark));
    redQueueDiscT1T2->TraceConnectWithoutContext("Drop", MakeCallback(&T1T2QueueDrop));


    // T2-R1 bottleneck link (1 Gbps with RED/ECN)
    NetDeviceContainer t2R1Devices;
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(T2_R1_RATE));
    p2pHelper.SetChannelAttribute("Delay", StringValue(T2_R1_DELAY));
    t2R1Devices = p2pHelper.Install(t2Node.Get(0), r1Node.Get(0));
    ipv4.SetBase("10.4.1.0", "255.255.255.252");
    ipv4.Assign(t2R1Devices);
    
    // Install RED QueueDisc on T2's outgoing interface (T2->R1)
    tcHelper.SetRootQueueDisc("ns3::RedQueueDisc",
                                "MaxBytes", UintegerValue(RED_Q_MAX_BYTES_T2_R1),
                                "MinThresh", DoubleValue(RED_MIN_THRESH_T2_R1),
                                "MaxThresh", DoubleValue(RED_MAX_THRESH_T2_R1),
                                "QW", DoubleValue(RED_QW_T2_R1));
    tcHelper.Install(t2R1Devices.Get(0)); // Install on the T2 side of the link (output queue)

    // Get pointer to the RedQueueDisc for tracing
    Ptr<QueueDisc> queueDiscT2R1 = DynamicCast<PointToPointNetDevice>(t2R1Devices.Get(0))->GetQueueDisc();
    NS_ASSERT_MSG(queueDiscT2R1 != nullptr, "QueueDisc not found on T2->R1 device");
    Ptr<RedQueueDisc> redQueueDiscT2R1 = DynamicCast<RedQueueDisc>(queueDiscT2R1);
    NS_ASSERT_MSG(redQueueDiscT2R1 != nullptr, "RedQueueDisc not found on T2->R1 device");
    redQueueDiscT2R1->TraceConnectWithoutContext("Mark", MakeCallback(&T2R1QueueMark));
    redQueueDiscT2R1->TraceConnectWithoutContext("Drop", MakeCallback(&T2R1QueueDrop));


    // R1 to Receivers links (high-speed access links)
    ipv4.SetBase("10.5.0.0", "255.255.0.0"); // Subnet for R1-D connections
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(R1_TO_D_RATE));
    p2pHelper.SetChannelAttribute("Delay", StringValue(R1_TO_D_DELAY));
    tcHelper.SetRootQueueDisc("ns3::NoopQueueDisc");
    for (uint32_t i = 0; i < N_TOTAL_FLOWS; ++i)
    {
        NetDeviceContainer devices = p2pHelper.Install(r1Node.Get(0), receiverNodes.Get(i));
        ipv4.Assign(devices);
        tcHelper.Install(devices.Get(0)); // R1 side
        tcHelper.Install(devices.Get(1)); // Receiver side
    }

    // Install IP stack on all nodes
    InternetStackHelper stack;
    stack.Install(sendersB1Nodes);
    stack.Install(sendersB2Nodes);
    stack.Install(t1Node);
    stack.Install(t2Node);
    stack.Install(r1Node);
    stack.Install(receiverNodes);

    // Populate routing tables using Global Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 4. Applications (TCP Flows)
    ApplicationContainer clientApps, serverApps;
    std::vector<Ptr<PacketSink>> packetSinks(N_TOTAL_FLOWS);
    uint32_t currentReceiverIdx = 0;

    // Install flows for sendersB1 (passing through T1-T2 bottleneck)
    for (uint32_t i = 0; i < N_FLOWS_B1; ++i)
    {
        // Receiver (PacketSink)
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), 9000 + i));
        serverApps.Add(sinkHelper.Install(receiverNodes.Get(currentReceiverIdx)));
        packetSinks[currentReceiverIdx] = DynamicCast<PacketSink>(serverApps.Get(serverApps.GetN() - 1));

        // Sender (BulkSendApplication)
        BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(receiverNodes.Get(currentReceiverIdx)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 9000 + i));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely
        clientApps.Add(clientHelper.Install(sendersB1Nodes.Get(i)));
        
        // Staggered start time within the startup window
        double startTime = flowStartTimeRng->GetValue();
        clientApps.Get(clientApps.GetN() - 1)->SetStartTime(Seconds(SIM_START_TIME + startTime));
        clientApps.Get(clientApps.GetN() - 1)->SetStopTime(Seconds(SIM_END_TIME));
        serverApps.Get(serverApps.GetN() - 1)->SetStartTime(Seconds(SIM_START_TIME));
        serverApps.Get(serverApps.GetN() - 1)->SetStopTime(Seconds(SIM_END_TIME));
        
        currentReceiverIdx++;
    }

    // Install flows for sendersB2 (passing through T2-R1 bottleneck directly)
    for (uint32_t i = 0; i < N_FLOWS_B2; ++i)
    {
        // Receiver (PacketSink)
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), 9000 + N_FLOWS_B1 + i));
        serverApps.Add(sinkHelper.Install(receiverNodes.Get(currentReceiverIdx)));
        packetSinks[currentReceiverIdx] = DynamicCast<PacketSink>(serverApps.Get(serverApps.GetN() - 1));

        // Sender (BulkSendApplication)
        BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(receiverNodes.Get(currentReceiverIdx)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 9000 + N_FLOWS_B1 + i));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely
        clientApps.Add(clientHelper.Install(sendersB2Nodes.Get(i)));

        // Staggered start time within the startup window
        double startTime = flowStartTimeRng->GetValue();
        clientApps.Get(clientApps.GetN() - 1)->SetStartTime(Seconds(SIM_START_TIME + startTime));
        clientApps.Get(clientApps.GetN() - 1)->SetStopTime(Seconds(SIM_END_TIME));
        serverApps.Get(serverApps.GetN() - 1)->SetStartTime(Seconds(SIM_START_TIME));
        serverApps.Get(serverApps.GetN() - 1)->SetStopTime(Seconds(SIM_END_TIME));
        
        currentReceiverIdx++;
    }

    // Schedule collection of startRxBytes for throughput measurement interval
    std::vector<uint64_t> startRxBytes(N_TOTAL_FLOWS);
    Simulator::Schedule(Seconds(MEASUREMENT_START_TIME), [&]() {
        for (uint32_t i = 0; i < N_TOTAL_FLOWS; ++i)
        {
            startRxBytes[i] = packetSinks[i]->GetTotalRx();
        }
    });

    // Schedule collection of endRxBytes for throughput measurement interval
    std::vector<uint64_t> endRxBytes(N_TOTAL_FLOWS);
    Simulator::Schedule(Seconds(MEASUREMENT_START_TIME + MEASUREMENT_INTERVAL), [&]() {
        for (uint32_t i = 0; i < N_TOTAL_FLOWS; ++i)
        {
            endRxBytes[i] = packetSinks[i]->GetTotalRx();
        }
    });

    // 5. Flow Monitor (Install before simulation run for comprehensive stats)
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();
    flowMonitor->CheckFor = FlowMonitor::CHECK_TX_RX; // Ensure Tx/Rx counts are collected

    // 6. Simulation Execution
    Simulator::Stop(Seconds(SIM_END_TIME));
    Simulator::Run();

    // 7. Statistics Collection and Output
    flowMonitor->Stop(); // Stop collecting stats now that simulation is done
    std::map<FlowId, FlowMonitorHelper::FlowStats> fmStats = flowMonitor->GetFlowStats();

    double totalMeasuredThroughput = 0;
    std::vector<double> flowThroughputs;

    NS_LOG_INFO("--- Flow Throughput (during [" << MEASUREMENT_START_TIME << "s, " << MEASUREMENT_START_TIME + MEASUREMENT_INTERVAL << "s]) ---");
    // Iterate through flows using our sink indices to align with collected byte counts
    for (uint32_t i = 0; i < N_TOTAL_FLOWS; ++i)
    {
        uint64_t rxBytesInterval = endRxBytes[i] - startRxBytes[i];
        double throughputMbps = (rxBytesInterval * 8.0) / (MEASUREMENT_INTERVAL * 1e6); // Calculate in Mbps

        // Attempt to find the corresponding FlowMonitor entry for additional details
        // (e.g., lost packets, total RxBytes, etc. for the entire simulation duration)
        // Flow IDs often start from 1 and align with application installation order.
        FlowId currentFlowId = i + 1; 
        Ipv4FlowClassifier::FiveTuple t;
        if(fmStats.count(currentFlowId))
        {
            t = flowHelper.GetClassifier()->GetFlowDescription(currentFlowId);
            NS_LOG_INFO("Flow ID: " << currentFlowId << " (" << t.sourceAddress << ":" << t.sourcePort
                                   << " -> " << t.destinationAddress << ":" << t.destinationPort << ")");
            NS_LOG_INFO("  Tx Packets (total sim): " << fmStats[currentFlowId].txPackets);
            NS_LOG_INFO("  Rx Packets (total sim): " << fmStats[currentFlowId].rxPackets);
            NS_LOG_INFO("  Lost Packets (total sim): " << fmStats[currentFlowId].lostPackets); // Lost includes packets dropped by network or reordered
        } else {
             NS_LOG_INFO("Flow (sink index " << i << ") - No FlowMonitor entry found for Flow ID " << currentFlowId);
        }

        NS_LOG_INFO("  Throughput (measured interval): " << throughputMbps << " Mbps");
        
        if (rxBytesInterval > 0) // Only include flows that transmitted data in the fairness calculation
        {
            flowThroughputs.push_back(throughputMbps);
            totalMeasuredThroughput += throughputMbps;
        }
    }

    // Calculate Jain's Fairness Index
    double jainFairnessIndex = 0;
    if (!flowThroughputs.empty())
    {
        double sum_x_i = 0;
        double sum_x_i_sq = 0;
        for (double tp : flowThroughputs)
        {
            sum_x_i += tp;
            sum_x_i_sq += (tp * tp);
        }

        if (sum_x_i_sq > 0)
        {
            jainFairnessIndex = (sum_x_i * sum_x_i) / (flowThroughputs.size() * sum_x_i_sq);
        }
    }

    NS_LOG_INFO("--- Aggregate Statistics ---");
    NS_LOG_INFO("Total Active Flows (in measurement interval): " << flowThroughputs.size());
    NS_LOG_INFO("Total Aggregate Throughput (Measured Interval): " << totalMeasuredThroughput << " Mbps");
    NS_LOG_INFO("Jain's Fairness Index: " << jainFairnessIndex);

    NS_LOG_INFO("--- Queue Statistics ---");
    NS_LOG_INFO("T1-T2 Link (10 Gbps) Queue:");
    NS_LOG_INFO("  Marked Packets (ECN): " << g_t1T2MarkedPackets);
    NS_LOG_INFO("  Dropped Packets: " << g_t1T2DroppedPackets);
    
    NS_LOG_INFO("T2-R1 Link (1 Gbps) Queue:");
    NS_LOG_INFO("  Marked Packets (ECN): " << g_t2R1MarkedPackets);
    NS_LOG_INFO("  Dropped Packets: " << g_t2R1DroppedPackets);
    
    Simulator::Destroy();
    return 0;
}