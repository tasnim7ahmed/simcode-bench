#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ping-helper.h"
#include "ns3/stats-module.h"

#include <iostream>
#include <fstream>
#include <string>

// Global parameters for easy configuration
#define SIM_STOP_TIME 10.0 // seconds
#define ENABLE_DCTCP false // Set to true to enable DCTCP and related tracing
#define ENABLE_PCAP true // Set to true to enable PCAP tracing
#define NUM_BULK_SEND_APPS 1 // Number of bulk send applications

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FqCoDelSimulation");

// --- Tracing Callbacks ---

// Callback for TCP Cwnd changes
void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

// Callback for TCP RTT changes
void RttChange(Ptr<OutputStreamWrapper> stream, Time rtt)
{
    *stream << Simulator::Now().GetSeconds() << "\t" << rtt.GetMilliSeconds() << std::endl;
}

// Callback for throughput monitoring
void ThroughputMonitor(Ptr<PacketSink> sink, Ptr<OutputStreamWrapper> stream)
{
    // Calculate throughput in Mbps
    double currentThroughput = sink->GetTotalRxBytes() * 8.0 / (Simulator::Now().GetSeconds() * 1000000.0);
    *stream << Simulator::Now().GetSeconds() << "\t" << currentThroughput << std::endl;
    Simulator::Schedule(Seconds(0.1), &ThroughputMonitor, sink, stream); // Schedule next sample
}

// Callback for QueueDisc events (Enqueue, Dequeue, Drop, Mark)
void QueueDiscEvent(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p, const std::string& eventType)
{
    *stream << Simulator::Now().GetSeconds() << "\t" << eventType << "\t" << p->GetUid() << std::endl;
}

// Callback for polling QueueDisc state (packets and bytes in queue) and detailed stats
void QueueStateTracer(Ptr<QueueDisc> qd, Ptr<OutputStreamWrapper> queueStateStream, Ptr<OutputStreamWrapper> queueStatsStream)
{
    QueueDisc::Stats stats = qd->GetStats();
    
    // Log current queue length and bytes
    *queueStateStream << Simulator::Now().GetSeconds() << "\t" << qd->GetNPacketsInQueue() << "\t" << qd->GetNBytesInQueue() << std::endl;
    
    // Log cumulative queue statistics
    *queueStatsStream << Simulator::Now().GetSeconds() << "\t" << stats.GetNPacketsEnqueued() << "\t" << stats.GetNPacketsDequeued()
                      << "\t" << stats.GetNPacketsDropped() << "\t" << stats.GetNPacketsMarked() << std::endl;

    Simulator::Schedule(Seconds(0.1), &QueueStateTracer, qd, queueStateStream, queueStatsStream); // Schedule next sample
}

// Callback for DCTCP ECN MarkedBytes (sender side)
void MarkedBytesChange(Ptr<OutputStreamWrapper> stream, uint32_t oldMarkedBytes, uint32_t newMarkedBytes)
{
    *stream << Simulator::Now().GetSeconds() << "\t" << newMarkedBytes << std::endl;
}

// Callback for DCTCP ECN CongestionState (sender side)
void EcnCongestionStateChange(Ptr<OutputStreamWrapper> stream, uint32_t oldState, uint32_t newState)
{
    *stream << Simulator::Now().GetSeconds() << "\t" << newState << std::endl;
}


int main(int argc, char* argv[])
{
    // Log setup for relevant components
    LogComponentEnable("FqCoDelSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("PingApplication", LOG_LEVEL_INFO);
    LogComponentEnable("FqCoDelQueueDisc", LOG_LEVEL_INFO);
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("TrafficControlHelper", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // --- Configure Attributes ---
    // General TCP type
    if (ENABLE_DCTCP)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));
        Config::SetDefault("ns3::TcpDctcp::EcnMode", StringValue("Explicit")); 
        Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    }
    else
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
        Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    }

    // FqCoDelQueueDisc Attributes
    Config::SetDefault("ns3::FqCoDelQueueDisc::DynamicLimit", BooleanValue(true));
    Config::SetDefault("ns3::FqCoDelQueueDisc::Limit", UintegerValue(1000)); // Packets
    Config::SetDefault("ns3::FqCoDelQueueDisc::Interval", TimeValue(NanoSeconds(100000))); // 100 ms
    Config::SetDefault("ns3::FqCoDelQueueDisc::Target", TimeValue(NanoSeconds(5000))); // 5 ms
    Config::SetDefault("ns3::FqCoDelQueueDisc::Quantum", UintegerValue(1514)); // bytes (MTU)
    Config::SetDefault("ns3::FqCoDelQueueDisc::EnableEcn", BooleanValue(true)); // Enable ECN marking for CoDel

    // Point-to-Point device attributes for regular links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    // Bottleneck link attributes
    PointToPointHelper bottleneckP2p;
    bottleneckP2p.SetDeviceAttribute("DataRate", StringValue("10Mbps")); // Bottleneck
    bottleneckP2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // --- Network Topology ---
    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(4); // n0 (Client) -> n1 (Router) -> n2 (Router) -> n3 (Server)

    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2)); // Bottleneck link
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    NS_LOG_INFO("Installing Point-to-Point devices.");
    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d1d2 = bottleneckP2p.Install(n1n2); // Install bottleneck link
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // --- Install Internet Stack ---
    NS_LOG_INFO("Installing Internet Stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    // --- Configure and Install Traffic Control (FqCoDel) ---
    NS_LOG_INFO("Configuring and Installing Traffic Control (FqCoDelQueueDisc).");
    TrafficControlHelper tc;
    // Set root queue disc to FqCoDelQueueDisc, inheriting attributes from Config::SetDefault
    tc.SetRootQueueDisc("ns3::FqCoDelQueueDisc");

    // Install FqCoDel on both devices of the bottleneck link
    tc.Install(d1d2); 

    // --- Assign IP Addresses ---
    NS_LOG_INFO("Assigning IPv4 Addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2); // Bottleneck link

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    // --- Global Routing ---
    NS_LOG_INFO("Populating global routing tables.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- Applications ---
    // 1. Ping Application (Client n0 to Server n3)
    NS_LOG_INFO("Setting up Ping application.");
    Ptr<Node> clientNode = nodes.Get(0);
    Ptr<Node> serverNode = nodes.Get(3);
    Ipv4Address serverIp = i2i3.GetAddress(1); // IP of n3 on the n2-n3 link

    PingHelper pingHelper;
    pingHelper.SetAttribute("PacketSize", UintegerValue(100));
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Count", UintegerValue(10)); // Send 10 ping requests
    ApplicationContainer pingApps = pingHelper.Install(clientNode, serverIp);
    pingApps.Start(Seconds(0.1));
    pingApps.Stop(Seconds(SIM_STOP_TIME - 0.1));

    // 2. BulkSend (TCP Client) to PacketSink (TCP Server)
    NS_LOG_INFO("Setting up BulkSend and PacketSink applications.");
    uint16_t port = 9; // Discard port for PacketSink
    Address sinkAddress(InetSocketAddress(serverIp, port));

    // PacketSink (Server on n3)
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(serverNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(SIM_STOP_TIME));
    Ptr<PacketSink> sink = StaticCast<PacketSink>(sinkApp.Get(0));

    // BulkSend (Client on n0)
    ApplicationContainer bulkSendApps;
    for (uint32_t i = 0; i < NUM_BULK_SEND_APPS; ++i)
    {
        BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", Address());
        bulkSendHelper.SetAttribute("Remote", AddressValue(sinkAddress));
        bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // 0 means unlimited bytes to send
        bulkSendHelper.SetAttribute("SendBufferSize", UintegerValue(1 << 20)); // 1MB send buffer

        bulkSendApps.Add(bulkSendHelper.Install(clientNode));
    }
    bulkSendApps.Start(Seconds(1.0));
    bulkSendApps.Stop(Seconds(SIM_STOP_TIME - 1.0));

    // --- Tracing ---
    NS_LOG_INFO("Configuring Tracing.");

    // PCAP tracing for all devices
    if (ENABLE_PCAP)
    {
        p2p.EnablePcapAll("fqcodel-sim"); 
        bottleneckP2p.EnablePcapAll("fqcodel-sim-bottleneck"); 
    }

    AsciiTraceHelper ascii;

    // 1. Queue Disc Tracing on Bottleneck Link
    // Get QueueDisc objects from the bottleneck devices (d1d2.Get(0) is n1's device, d1d2.Get(1) is n2's device)
    Ptr<QueueDisc> qd1 = tc.GetRootQueueDisc(d1d2.Get(0)); 
    Ptr<QueueDisc> qd2 = tc.GetRootQueueDisc(d1d2.Get(1)); 

    // File streams for queue state (packets and bytes in queue)
    Ptr<OutputStreamWrapper> qd1_queue_state_stream = ascii.CreateFileStream("qd1-queue-state.txt");
    Ptr<OutputStreamWrapper> qd2_queue_state_stream = ascii.CreateFileStream("qd2-queue-state.txt");
    *qd1_queue_state_stream << "Time\tPacketsInQueue\tBytesInQueue" << std::endl;
    *qd2_queue_state_stream << "Time\tPacketsInQueue\tBytesInQueue" << std::endl;

    // File streams for cumulative queue statistics (enqueued, dequeued, dropped, marked)
    Ptr<OutputStreamWrapper> qd1_queue_stats_stream = ascii.CreateFileStream("qd1-queue-stats.txt");
    Ptr<OutputStreamWrapper> qd2_queue_stats_stream = ascii.CreateFileStream("qd2-queue-stats.txt");
    *qd1_queue_stats_stream << "Time\tEnqueued\tDequeued\tDropped\tMarked" << std::endl;
    *qd2_queue_stats_stream << "Time\tEnqueued\tDequeued\tDropped\tMarked" << std::endl;

    // Schedule periodic polling for queue state and stats
    Simulator::Schedule(Seconds(0.0), &QueueStateTracer, qd1, qd1_queue_state_stream, qd1_queue_stats_stream);
    Simulator::Schedule(Seconds(0.0), &QueueStateTracer, qd2, qd2_queue_state_stream, qd2_queue_stats_stream);

    // File streams for individual queue disc events (drops, marks)
    Ptr<OutputStreamWrapper> qd1_events_stream = ascii.CreateFileStream("qd1-events.txt");
    Ptr<OutputStreamWrapper> qd2_events_stream = ascii.CreateFileStream("qd2-events.txt");

    // Connect to trace sources for individual packet events on QueueDisc
    qd1->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&QueueDiscEvent, qd1_events_stream, "Enqueue"));
    qd1->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(&QueueDiscEvent, qd1_events_stream, "Dequeue"));
    qd1->TraceConnectWithoutContext("Drop", MakeBoundCallback(&QueueDiscEvent, qd1_events_stream, "Drop"));
    qd1->TraceConnectWithoutContext("Mark", MakeBoundCallback(&QueueDiscEvent, qd1_events_stream, "Mark"));

    qd2->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&QueueDiscEvent, qd2_events_stream, "Enqueue"));
    qd2->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(&QueueDiscEvent, qd2_events_stream, "Dequeue"));
    qd2->TraceConnectWithoutContext("Drop", MakeBoundCallback(&QueueDiscEvent, qd2_events_stream, "Drop"));
    qd2->TraceConnectWithoutContext("Mark", MakeBoundCallback(&QueueDiscEvent, qd2_events_stream, "Mark"));


    // 2. TCP Tracing (RTT, CWND)
    Ptr<OutputStreamWrapper> cwndStream = ascii.CreateFileStream("cwnd.tr");
    Ptr<OutputStreamWrapper> rttStream = ascii.CreateFileStream("rtt.tr");
    
    // Connect to all TCP sockets' CongestionWindow and RTT trace sources on Node 0 (client node)
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow", MakeBoundCallback(&CwndChange, cwndStream));
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/RTT", MakeBoundCallback(&RttChange, rttStream));

    // 3. Throughput Tracing (PacketSink)
    Ptr<OutputStreamWrapper> throughputStream = ascii.CreateFileStream("throughput.tr");
    *throughputStream << "Time\tThroughput(Mbps)" << std::endl;
    // Schedule periodic throughput sampling from the PacketSink on the server
    Simulator::Schedule(Seconds(0.0), &ThroughputMonitor, sink, throughputStream);

    // 4. DCTCP Specific Tracing (if enabled)
    if (ENABLE_DCTCP)
    {
        Ptr<OutputStreamWrapper> markedBytesStream = ascii.CreateFileStream("dctcp-marked-bytes.tr");
        Ptr<OutputStreamWrapper> ecnCongestionStateStream = ascii.CreateFileStream("dctcp-ecn-state.tr");

        // Connect to DCTCP specific trace sources on Node 0 (client side)
        Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/MarkedBytes", MakeBoundCallback(&MarkedBytesChange, markedBytesStream));
        Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*/EcnCongestionState", MakeBoundCallback(&EcnCongestionStateChange, ecnCongestionStateStream));
    }


    // --- Simulation Run ---
    NS_LOG_INFO("Starting simulation for " << SIM_STOP_TIME << " seconds.");
    Simulator::Stop(Seconds(SIM_STOP_TIME));
    Simulator::Run();
    Simulator::Destroy(); // This cleans up all resources and closes streams
    NS_LOG_INFO("Simulation finished.");

    return 0;
}