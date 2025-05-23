#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h" 
#include "ns3/tcp-socket-base.h"
#include <map>
#include <string>
#include <fstream>
#include <vector>

using namespace ns3;

// Global trace streams and flow ID management
static std::map<uint32_t, Ptr<OutputStreamWrapper>> g_cwndStreams;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> g_ssThreshStreams;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> g_rttStreams;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> g_rtoStreams;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> g_inFlightStreams;
static uint32_t g_nextFlowId = 0;

/**
 * \brief Callback for Congestion Window (Cwnd) change.
 * \param stream Output stream to write to.
 * \param oldCwnd Old Cwnd value.
 * \param newCwnd New Cwnd value.
 */
static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

/**
 * \brief Callback for Slow Start Threshold (SsThresh) change.
 * \param stream Output stream to write to.
 * \param oldSsThresh Old SsThresh value.
 * \param newSsThresh New SsThresh value.
 */
static void
SsThreshChange(Ptr<OutputStreamWrapper> stream, uint32_t oldSsThresh, uint32_t newSsThresh)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newSsThresh << std::endl;
}

/**
 * \brief Callback for RTT change.
 * \param stream Output stream to write to.
 * \param oldRtt Old RTT value.
 * \param newRtt New RTT value.
 */
static void
RttChange(Ptr<OutputStreamWrapper> stream, Time oldRtt, Time newRtt)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newRtt.GetMilliSeconds() << std::endl;
}

/**
 * \brief Callback for RTO change.
 * \param stream Output stream to write to.
 * \param oldRto Old RTO value.
 * \param newRto New RTO value.
 */
static void
RtoChange(Ptr<OutputStreamWrapper> stream, Time oldRto, Time newRto)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newRto.GetMilliSeconds() << std::endl;
}

/**
 * \brief Callback for Bytes In Flight change.
 * \param stream Output stream to write to.
 * \param oldInFlight Old Bytes In Flight value.
 * \param newInFlight New Bytes In Flight value.
 */
static void
InFlightChange(Ptr<OutputStreamWrapper> stream, uint32_t oldInFlight, uint32_t newInFlight)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newInFlight << std::endl;
}

/**
 * \brief Attaches TCP trace sources to a given socket.
 * \param socket The TCP socket to trace.
 */
static void
AttachTcpTraces(Ptr<Socket> socket)
{
    Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
    if (!tcpSocket)
    {
        return; // Not a TCP socket
    }

    uint32_t flowId = g_nextFlowId++;
    AsciiTraceHelper ascii;

    std::string cwndFileName = "cwnd-flow-" + std::to_string(flowId) + ".txt";
    g_cwndStreams[flowId] = ascii.CreateFileStream(cwndFileName);
    tcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&CwndChange), g_cwndStreams[flowId]);

    std::string ssThreshFileName = "ssThresh-flow-" + std::to_string(flowId) + ".txt";
    g_ssThreshStreams[flowId] = ascii.CreateFileStream(ssThreshFileName);
    tcpSocket->TraceConnectWithoutContext("SlowStartThreshold", MakeCallback(&SsThreshChange), g_ssThreshStreams[flowId]);

    std::string rttFileName = "rtt-flow-" + std::to_string(flowId) + ".txt";
    g_rttStreams[flowId] = ascii.CreateFileStream(rttFileName);
    tcpSocket->TraceConnectWithoutContext("Rtt", MakeCallback(&RttChange), g_rttStreams[flowId]);

    std::string rtoFileName = "rto-flow-" + std::to_string(flowId) + ".txt";
    g_rtoStreams[flowId] = ascii.CreateFileStream(rtoFileName);
    tcpSocket->TraceConnectWithoutContext("Rto", MakeCallback(&RtoChange), g_rtoStreams[flowId]);

    std::string inFlightFileName = "inFlight-flow-" + std::to_string(flowId) + ".txt";
    g_inFlightStreams[flowId] = ascii.CreateFileStream(inFlightFileName);
    tcpSocket->TraceConnectWithoutContext("BytesInFlight", MakeCallback(&InFlightChange), g_inFlightStreams[flowId]);
}


int main(int argc, char *argv[])
{
    // 1. Set default values and parse command-line arguments
    SimulationTime simulationTime = Seconds(60);
    std::string tcpVariant = "ns3::TcpWestwoodPlus";
    std::string bottleneckDataRate = "10Mbps";
    std::string bottleneckDelay = "10ms";
    double errorRate = 0.0;
    uint32_t numFlows = 1;
    uint32_t packetSize = 1000; // bytes
    std::string queueDiscType = "ns3::DropTailQueue";
    uint32_t queueSizePackets = 100; // Packets

    bool enablePcap = false;
    bool enableFlowMonitor = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("tcpVariant", "TCP variant to use (e.g., ns3::TcpNewReno, ns3::TcpWestwoodPlus)", tcpVariant);
    cmd.AddValue("bottleneckDataRate", "Data rate of the bottleneck link", bottleneckDataRate);
    cmd.AddValue("bottleneckDelay", "Delay of the bottleneck link", bottleneckDelay);
    cmd.AddValue("errorRate", "Packet error rate on the bottleneck link (0.0 to 1.0)", errorRate);
    cmd.AddValue("numFlows", "Number of concurrent TCP flows", numFlows);
    cmd.AddValue("packetSize", "Size of TCP packets in bytes", packetSize);
    cmd.AddValue("queueDiscType", "Queue discipline type for the bottleneck (e.g., ns3::DropTailQueue)", queueDiscType);
    cmd.AddValue("queueSizePackets", "Queue size in packets for the bottleneck link", queueSizePackets);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableFlowMonitor", "Enable FlowMonitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // 2. Configure TCP Variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue(tcpVariant));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(10 * 1024 * 1024)); // 10MB
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(10 * 1024 * 1024)); // 10MB

    // 3. Network Topology (Source-Router-Sink)
    NodeContainer sourceNodes;
    sourceNodes.Create(numFlows); // One source node per flow
    NodeContainer routerNode;
    routerNode.Create(1);
    NodeContainer sinkNode;
    sinkNode.Create(1);

    PointToPointHelper p2pRouterToSink;
    p2pRouterToSink.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    p2pRouterToSink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    PointToPointHelper p2pSourceToRouter;
    // Assume sufficient bandwidth for source-router links to not be the bottleneck
    p2pSourceToRouter.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pSourceToRouter.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer routerDevices;
    NetDeviceContainer sinkDevices;

    // Connect router to sink (bottleneck link)
    routerDevices = p2pRouterToSink.Install(routerNode.Get(0), sinkNode.Get(0));
    sinkDevices = p2pRouterToSink.Install(sinkNode.Get(0), routerNode.Get(0));

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(sourceNodes);
    stack.Install(routerNode);
    stack.Install(sinkNode);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerSinkInterfaces = address.Assign(routerDevices);
    Ipv4InterfaceContainer sinkRouterInterfaces = address.Assign(sinkDevices); // Assign to the other side too

    std::vector<NetDeviceContainer> sourceRouterDevices(numFlows);
    std::vector<Ipv4InterfaceContainer> sourceRouterInterfaces(numFlows);

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        sourceRouterDevices[i] = p2pSourceToRouter.Install(sourceNodes.Get(i), routerNode.Get(0));
        address.SetBase("10.1." + std::to_string(i + 2) + ".0", "255.255.255.0");
        sourceRouterInterfaces[i] = address.Assign(sourceRouterDevices[i]);
    }

    // Add error model if specified
    if (errorRate > 0)
    {
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(errorRate));
        routerDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
        sinkDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    // 4. Traffic Control (Queues) on Bottleneck Link
    TrafficControlHelper tcHelper;
    tcHelper.Set  ("ns3::QueueDisc", "MaxSize", StringValue(std::to_string(queueSizePackets) + "p"));
    tcHelper.Set  ("ns3::QueueDisc", "Scheduler", StringValue("ns3::PfifoFastQueueDisc")); 

    // Install queue discipline for the bottleneck devices
    tcHelper.Install(routerDevices.Get(0)); // Egress from router towards sink
    tcHelper.Install(sinkDevices.Get(0));   // Egress from sink towards router (for ACKs)


    // 5. Applications (BulkSend and PacketSink)
    std::vector<Ptr<BulkSendApplication>> bulkSendApps(numFlows);
    std::vector<Ptr<PacketSink>> packetSinkApps(numFlows);

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        // Sink application
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                          InetSocketAddress(Ipv4Address::GetAny(), 9000 + i));
        ApplicationContainer sinkApp = packetSinkHelper.Install(sinkNode);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(simulationTime);
        packetSinkApps[i] = DynamicCast<PacketSink>(sinkApp.Get(0));

        // Source application
        BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(sinkRouterInterfaces.GetAddress(0), 9000 + i));
        // Set MaxBytes to 0 for unlimited
        bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
        bulkSendHelper.SetAttribute("SendSize", UintegerValue(packetSize));
        ApplicationContainer sourceApp = bulkSendHelper.Install(sourceNodes.Get(i));
        sourceApp.Start(Seconds(1.0 + i * 0.1)); // Stagger start times slightly
        sourceApp.Stop(simulationTime);
        bulkSendApps[i] = DynamicCast<BulkSendApplication>(sourceApp.Get(0));

        // Attach TCP traces to the socket
        Ptr<Socket> tcpSocket = bulkSendApps[i]->GetSocket();
        AttachTcpTraces(tcpSocket);
    }

    // 6. Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 7. Tracing (PCAP)
    if (enablePcap)
    {
        p2pRouterToSink.EnablePcapAll("westwood-bottleneck");
        for (uint32_t i = 0; i < numFlows; ++i)
        {
            p2pSourceToRouter.EnablePcap("westwood-source-" + std::to_string(i), sourceNodes.Get(i)->GetId(), 0);
        }
    }

    // 8. Flow Monitor
    Ptr<FlowMonitor> flowMonitor;
    if (enableFlowMonitor)
    {
        FlowMonitorHelper flowMonitorHelper;
        flowMonitor = flowMonitorHelper.InstallAll();
    }

    // 9. Run simulation
    Simulator::Stop(simulationTime);
    Simulator::Run();

    // 10. Process results (Flow Monitor)
    if (enableFlowMonitor)
    {
        flowMonitor->CheckFor<FlowMonitor::FlowStats>();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->Get
<FlowMonitor::FlowStats>();

        double totalThroughputMbps = 0.0;
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
            std::cout << "  Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "    Tx Bytes: " << i->second.txBytes << std::endl;
            std::cout << "    Rx Bytes: " << i->second.rxBytes << std::endl;
            std::cout << "    Tx Packets: " << i->second.txPackets << std::endl;
            std::cout << "    Rx Packets: " << i->second.rxPackets << std::endl;
            std::cout << "    Lost Packets: " << i->second.lostPackets << std::endl;
            std::cout << "    Delay Sum: " << i->second.delaySum.GetSeconds() << std::endl;
            if (i->second.rxPackets > 0)
            {
                std::cout << "    Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << std::endl;
                std::cout << "    Jitter Sum: " << i->second.jitterSum.GetSeconds() << std::endl;
                std::cout << "    Mean Jitter: " << i->second.jitterSum.GetSeconds() / i->second.rxPackets << std::endl;
            }
            double appThroughput = (double)i->second.rxBytes * 8 / (simulationTime.GetSeconds() * 1000 * 1000);
            std::cout << "    Throughput: " << appThroughput << " Mbps" << std::endl;
            totalThroughputMbps += appThroughput;
        }
        std::cout << "Total Throughput: " << totalThroughputMbps << " Mbps" << std::endl;
    }

    Simulator::Destroy();

    return 0;
}