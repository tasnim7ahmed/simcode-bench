#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/rtt-probe-helper.h"
#include "ns3/tcp-westwood-plus.h" // Include for setting TCP type
#include "ns3/bulk-send-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/qos-module.h" // For QueueDisc helpers like PfifoFast, ARED, CoDel, FqCoDel, PIE

#include <fstream>
#include <string>

using namespace ns3;

// Global file streams for tracing
std::ofstream g_rttTraceFile;
std::ofstream g_goodputFile;
std::ofstream g_queueBytesFile;

// Global variables for goodput monitoring
static uint64_t g_prevRxBytes13 = 0;
static uint64_t g_prevRxBytes31 = 0;
static Time g_lastGoodputTime = Seconds(0.0);

// Global variables for queue monitoring
static Ptr<QueueDisc> g_bottleneckQueueDisc;

// Trace RTT callback for V4Ping
void TraceRtt(Time rtt)
{
    g_rttTraceFile << Simulator::Now().GetSeconds() << "\t" << rtt.GetMilliSeconds() << std::endl;
}

// Monitor goodput periodically
void MonitorGoodput(Ptr<PacketSink> sink13, Ptr<PacketSink> sink31, double simTime, double interval)
{
    if (Simulator::Now().GetSeconds() >= simTime)
    {
        return; // Stop monitoring at simulation end
    }

    uint64_t currentRxBytes13 = sink13->GetTotalRx();
    uint64_t currentRxBytes31 = sink31->GetTotalRx();
    Time currentTime = Simulator::Now();

    double elapsedSec = (currentTime - g_lastGoodputTime).GetSeconds();

    // Calculate goodput if enough time has passed
    if (elapsedSec > 0.0)
    {
        double goodput13Mbps = (currentRxBytes13 - g_prevRxBytes13) * 8.0 / (elapsedSec * 1000000.0);
        double goodput31Mbps = (currentRxBytes31 - g_prevRxBytes31) * 8.0 / (elapsedSec * 1000000.0);
        g_goodputFile << currentTime.GetSeconds() << "\t" << goodput13Mbps << "\t" << goodput31Mbps << std::endl;
    }

    g_prevRxBytes13 = currentRxBytes13;
    g_prevRxBytes31 = currentRxBytes31;
    g_lastGoodputTime = currentTime;

    Simulator::Schedule(Seconds(interval), &MonitorGoodput, sink13, sink31, simTime, interval);
}

// Monitor queue bytes periodically
void MonitorQueueDisc(Ptr<QueueDisc> qd, double simTime, double interval)
{
    if (Simulator::Now().GetSeconds() >= simTime)
    {
        return;
    }

    g_queueBytesFile << Simulator::Now().GetSeconds() << "\t" << qd->GetNBytesInQueue() << std::endl;

    Simulator::Schedule(Seconds(interval), &MonitorQueueDisc, qd, simTime, interval);
}

int main(int argc, char *argv[])
{
    // 1. CommandLine Arguments
    std::string bottleneckBandwidth = "10Mbps";
    std::string bottleneckDelay = "10ms";
    std::string queueDiscType = "FqCoDel"; // Default queue disc type
    bool enableBQL = false;
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1460;   // bytes (TCP segment size)
    bool pcapTracing = false;
    bool rttTracing = true;
    double goodputMeasurementInterval = 0.1; // seconds
    double queueMeasurementInterval = 0.1;   // seconds
    uint32_t bqlByteLimit = 1000000; // Default BQL limit (1MB)

    CommandLine cmd(__FILE__);
    cmd.AddValue("bottleneckBandwidth", "Bandwidth of the bottleneck link (n2-n3)", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Delay of the bottleneck link (n2-n3)", bottleneckDelay);
    cmd.AddValue("queueDiscType", "Type of Queue Disc (PfifoFast, ARED, CoDel, FqCoDel, PIE)", queueDiscType);
    cmd.AddValue("enableBQL", "Enable Byte Queue Limits (BQL) on the bottleneck link", enableBQL);
    cmd.AddValue("bqlByteLimit", "Byte limit for BQL if enabled", bqlByteLimit);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("packetSize", "TCP segment size in bytes", packetSize);
    cmd.AddValue("pcapTracing", "Enable or disable Pcap tracing", pcapTracing);
    cmd.AddValue("rttTracing", "Enable or disable RTT tracing from ping", rttTracing);
    cmd.Parse(argc, argv);

    // Set TCP default attributes
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
    // Set large send/receive buffers to avoid being limited by them
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(13107200)); // 10MB approx
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(13107200)); // 10MB approx

    // 2. Node Creation
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> n1 = nodes.Get(0);
    Ptr<Node> n2 = nodes.Get(1);
    Ptr<Node> n3 = nodes.Get(2);

    // 3. Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // 4. Point-to-Point Links and IP Addressing
    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p12.SetChannelAttribute("Delay", StringValue("0.1ms"));
    NetDeviceContainer devices12 = p2p12.Install(n1, n2);

    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    p2p23.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    NetDeviceContainer devices23 = p2p23.Install(n2, n3);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = ipv4.Assign(devices12);
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = ipv4.Assign(devices23);

    // 5. Populate Routing Tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Queue Disc Configuration on Bottleneck Link (n2's interface facing n3)
    // We install the queue disc directly on the NetDevice using its helper.
    Ptr<QueueDisc> bottleneckQueueDisc;

    if (queueDiscType == "PfifoFast")
    {
        PfifoFastQueueDiscHelper pfifoFastHelper;
        if (enableBQL)
        {
            pfifoFastHelper.SetAttribute("LimitBytes", BooleanValue(true));
            pfifoFastHelper.SetAttribute("MaxBytes", UintegerValue(bqlByteLimit));
        }
        bottleneckQueueDisc = pfifoFastHelper.Install(devices23.Get(0));
    }
    else if (queueDiscType == "ARED")
    {
        AredQueueDiscHelper aredHelper;
        aredHelper.SetAttribute("Mode", StringValue("BYTES")); // Use byte mode for thresholds
        if (enableBQL)
        {
            aredHelper.SetAttribute("EnableByteLimit", BooleanValue(true));
            aredHelper.SetAttribute("ByteLimit", UintegerValue(bqlByteLimit));
        }
        bottleneckQueueDisc = aredHelper.Install(devices23.Get(0));
    }
    else if (queueDiscType == "CoDel")
    {
        CoDelQueueDiscHelper codelHelper;
        if (enableBQL)
        {
            codelHelper.SetAttribute("EnableByteLimit", BooleanValue(true));
            codelHelper.SetAttribute("ByteLimit", UintegerValue(bqlByteLimit));
        }
        bottleneckQueueDisc = codelHelper.Install(devices23.Get(0));
    }
    else if (queueDiscType == "FqCoDel")
    {
        FqCoDelQueueDiscHelper fqcodelHelper;
        if (enableBQL)
        {
            fqcodelHelper.SetAttribute("EnableByteLimit", BooleanValue(true));
            fqcodelHelper.SetAttribute("ByteLimit", UintegerValue(bqlByteLimit));
        }
        bottleneckQueueDisc = fqcodelHelper.Install(devices23.Get(0));
    }
    else if (queueDiscType == "PIE")
    {
        PieQueueDiscHelper pieHelper;
        if (enableBQL)
        {
            pieHelper.SetAttribute("EnableByteLimit", BooleanValue(true));
            pieHelper.SetAttribute("ByteLimit", UintegerValue(bqlByteLimit));
        }
        bottleneckQueueDisc = pieHelper.Install(devices23.Get(0));
    }
    else
    {
        NS_FATAL_ERROR("Unknown queue disc type: " << queueDiscType);
    }

    g_bottleneckQueueDisc = bottleneckQueueDisc; // Store for monitoring

    // 7. TCP Applications (BulkSend and PacketSink)
    // N1 -> N3 (forward flow)
    uint16_t sinkPortF = 8080;
    PacketSinkHelper packetSinkHelperF("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::Any, sinkPortF));
    ApplicationContainer sinkAppsF = packetSinkHelperF.Install(n3);
    sinkAppsF.Start(Seconds(0.0));
    sinkAppsF.Stop(Seconds(simulationTime + 0.5)); // Allow some buffer time

    BulkSendHelper bulkSendHelperF("ns3::TcpSocketFactory", InetSocketAddress(interfaces23.Get(1).GetLocalAddress(), sinkPortF));
    bulkSendHelperF.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely
    ApplicationContainer sourceAppsF = bulkSendHelperF.Install(n1);
    sourceAppsF.Start(Seconds(1.0));
    sourceAppsF.Stop(Seconds(simulationTime));

    // N3 -> N1 (reverse flow)
    uint16_t sinkPortR = 8081;
    PacketSinkHelper packetSinkHelperR("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::Any, sinkPortR));
    ApplicationContainer sinkAppsR = packetSinkHelperR.Install(n1);
    sinkAppsR.Start(Seconds(0.0));
    sinkAppsR.Stop(Seconds(simulationTime + 0.5));

    BulkSendHelper bulkSendHelperR("ns3::TcpSocketFactory", InetSocketAddress(interfaces12.Get(0).GetLocalAddress(), sinkPortR));
    bulkSendHelperR.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely
    ApplicationContainer sourceAppsR = bulkSendHelperR.Install(n3);
    sourceAppsR.Start(Seconds(1.0));
    sourceAppsR.Stop(Seconds(simulationTime));

    // 8. Ping Application (N1 -> N3 for RTT)
    V4PingHelper pingHelper(interfaces23.Get(1).GetLocalAddress()); // Ping N3's IP
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Size", UintegerValue(64));
    pingHelper.SetAttribute("Verbose", BooleanValue(false));
    ApplicationContainer pingApps = pingHelper.Install(n1);
    pingApps.Start(Seconds(1.0));
    pingApps.Stop(Seconds(simulationTime));

    // 9. Tracing and Monitoring Setup

    // Pcap tracing
    if (pcapTracing)
    {
        p2p12.EnablePcapAll("n1-n2-pcap");
        p2p23.EnablePcapAll("n2-n3-pcap");
    }

    // RTT tracing
    if (rttTracing)
    {
        std::string pingTraceFileName = "rtt-trace-" + queueDiscType + ".txt";
        g_rttTraceFile.open(pingTraceFileName.c_str());
        g_rttTraceFile << "Time\tRTT(ms)" << std::endl;

        Ptr<V4Ping> pingApp = DynamicCast<V4Ping>(pingApps.Get(0));
        pingApp->TraceConnectWithoutContext("Rtt", MakeCallback(&TraceRtt));
    }

    // Goodput tracing
    Ptr<PacketSink> sink13 = DynamicCast<PacketSink>(sinkAppsF.Get(0));
    Ptr<PacketSink> sink31 = DynamicCast<PacketSink>(sinkAppsR.Get(0));
    std::string goodputFileName = "goodput-" + queueDiscType + ".txt";
    g_goodputFile.open(goodputFileName.c_str());
    g_goodputFile << "Time\tGoodput_N1_N3(Mbps)\tGoodput_N3_N1(Mbps)" << std::endl;
    Simulator::Schedule(Seconds(goodputMeasurementInterval), &MonitorGoodput, sink13, sink31, simulationTime, goodputMeasurementInterval);

    // Queue bytes tracing
    std::string queueBytesFileName = "bytes-in-queue-" + queueDiscType + ".txt";
    g_queueBytesFile.open(queueBytesFileName.c_str());
    g_queueBytesFile << "Time\tBytesInQueue" << std::endl;
    Simulator::Schedule(Seconds(queueMeasurementInterval), &MonitorQueueDisc, g_bottleneckQueueDisc, simulationTime, queueMeasurementInterval);

    // Queue limit tracing (static value, logged once)
    std::string queueLimitFileName = "queue-limit-" + queueDiscType + ".txt";
    std::ofstream queueLimitFile(queueLimitFileName.c_str());
    uint32_t reportedQueueByteLimit = 0;
    if (enableBQL)
    {
        // Try to retrieve the actual attribute value from the installed queue disc
        if (g_bottleneckQueueDisc->HasAttribute("ByteLimit")) // for CoDel, FqCoDel, PIE, ARED
        {
            reportedQueueByteLimit = g_bottleneckQueueDisc->GetAttribute("ByteLimit").Get<UintegerValue>().Get();
        }
        else if (g_bottleneckQueueDisc->HasAttribute("MaxBytes")) // for PfifoFast
        {
            reportedQueueByteLimit = g_bottleneckQueueDisc->GetAttribute("MaxBytes").Get<UintegerValue>().Get();
        }
        else
        {
            reportedQueueByteLimit = bqlByteLimit; // Fallback to CommandLine value if attribute not found
        }
    }
    queueLimitFile << "ConfiguredQueueByteLimit\t" << reportedQueueByteLimit << std::endl;
    queueLimitFile.close();

    // Flow Monitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // 10. Simulation Execution
    Simulator::Stop(Seconds(simulationTime + 1.0)); // Ensure all applications stop gracefully
    Simulator::Run();

    // 11. Post-Simulation Analysis
    monitor->SerializeToXmlFile("flowmon-" + queueDiscType + ".xml", true, true);

    // Close global file streams
    if (g_rttTraceFile.is_open())
    {
        g_rttTraceFile.close();
    }
    if (g_goodputFile.is_open())
    {
        g_goodputFile.close();
    }
    if (g_queueBytesFile.is_open())
    {
        g_queueBytesFile.close();
    }

    Simulator::Destroy();

    return 0;
}