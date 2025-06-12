#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <cmath>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpFigure17Sim");

static std::map<uint32_t, double> flowRxBytes;
static std::map<uint32_t, Ptr<PacketSink>> flowSinks;

void
RxTrace (Ptr<const Packet> packet, const Address &address, uint32_t flowId)
{
    flowRxBytes[flowId] += packet->GetSize();
}

double
CalculateFairness(const std::vector<double> &throughputs)
{
    double sum = 0, sq_sum = 0;
    for (auto tp : throughputs)
    {
        sum += tp;
        sq_sum += tp * tp;
    }
    if (sum == 0) return 0.0;
    return (sum * sum) / (throughputs.size() * sq_sum);
}

int
main(int argc, char *argv[])
{
    uint32_t numFlows = 40;
    double simStart = 0.0;
    double simStop = 5.5;
    double startWindow = 1.0;
    double convergenceTime = 3.0;
    double measureStart = convergenceTime;
    double measureDuration = 1.0;

    // Bottleneck characteristics
    uint32_t T1T2Senders = 30;
    std::string T1T2Rate = "10Gbps";
    uint32_t T2R1Senders = 20;
    std::string T2R1Rate = "1Gbps";

    // Common attributes
    std::string accessRate = "40Gbps";
    std::string accessDelay = "10us";
    std::string switchDelay = "10us";
    std::string bottleneckDelay = "30us";
    uint32_t pktSize = 1460;

    // Nodes: S0-S39 (senders), R0-R39 (receivers), Edge switches (ES0, ES1), T1, T2, R1
    NodeContainer senders, receivers, edgeSwitches, aggSwitches;
    senders.Create(numFlows);
    receivers.Create(numFlows);
    edgeSwitches.Create(2);
    aggSwitches.Create(2); // T1, T2
    Ptr<Node> r1 = CreateObject<Node>();

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(receivers);
    stack.Install(edgeSwitches);
    stack.Install(aggSwitches);
    stack.Install(r1);

    // PointToPoint helpers
    PointToPointHelper accessP2p;
    accessP2p.SetDeviceAttribute("DataRate", StringValue(accessRate));
    accessP2p.SetChannelAttribute("Delay", StringValue(accessDelay));
    PointToPointHelper esT1, esT2;
    esT1.SetDeviceAttribute("DataRate", StringValue(accessRate));
    esT1.SetChannelAttribute("Delay", StringValue(switchDelay));
    esT2.SetDeviceAttribute("DataRate", StringValue(accessRate));
    esT2.SetChannelAttribute("Delay", StringValue(switchDelay));
    PointToPointHelper t1t2;
    t1t2.SetDeviceAttribute("DataRate", StringValue(T1T2Rate));
    t1t2.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    PointToPointHelper t2r1;
    t2r1.SetDeviceAttribute("DataRate", StringValue(T2R1Rate));
    t2r1.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    // Traffic Control and RED w/ ECN
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc", 
        "QueueLimit", UintegerValue(1000), // packets
        "MinTh", DoubleValue(65 * pktSize), // bytes
        "MaxTh", DoubleValue(130 * pktSize), // bytes
        "Mode", StringValue("QUEUE_MODE_BYTES"),
        "MeanPktSize", UintegerValue(pktSize),
        "LinkBandwidth", StringValue(T1T2Rate), // Will adjust per bottleneck
        "LinkDelay", StringValue(bottleneckDelay),
        "UseEcn", BooleanValue(true)
    );

    // IP address helpers
    Ipv4AddressHelper ipa;
    std::vector<Ipv4InterfaceContainer> senderIfs(numFlows);
    std::vector<Ipv4InterfaceContainer> receiverIfs(numFlows);
    Ipv4InterfaceContainer t1t2If, t2r1If;
    std::vector<NetDeviceContainer> accessDevs(numFlows);

    // ====== TOPOLOGY BUILDING =====
    // S0-19 connect to ES0, S20-39 to ES1
    for (uint32_t i = 0; i < numFlows; ++i)
    {
        Ptr<Node> sender = senders.Get(i);
        Ptr<Node> edge = (i < 20) ? edgeSwitches.Get(0) : edgeSwitches.Get(1);
        NetDeviceContainer link = accessP2p.Install(sender, edge);

        std::ostringstream subnet;
        subnet << "10." << (i+1) << ".0.0";
        ipa.SetBase(subnet.str().c_str(), "255.255.255.0");
        senderIfs[i] = ipa.Assign(link);
        accessDevs[i] = link;
    }

    // R0-19 connect to ES0, R20-39 to ES1
    for (uint32_t i = 0; i < numFlows; ++i)
    {
        Ptr<Node> receiver = receivers.Get(i);
        Ptr<Node> edge = (i < 20) ? edgeSwitches.Get(0) : edgeSwitches.Get(1);
        NetDeviceContainer link = accessP2p.Install(receiver, edge);

        std::ostringstream subnet;
        subnet << "10." << (i+1) << ".1.0";
        ipa.SetBase(subnet.str().c_str(), "255.255.255.0");
        receiverIfs[i] = ipa.Assign(link);
    }

    // Edge switches to Aggregation: ES0 connects to T1, ES1 to T2
    NetDeviceContainer es0t1 = esT1.Install(edgeSwitches.Get(0), aggSwitches.Get(0)); // ES0-T1
    NetDeviceContainer es1t2 = esT2.Install(edgeSwitches.Get(1), aggSwitches.Get(1)); // ES1-T2
    ipa.SetBase("172.16.0.0", "255.255.255.0");
    ipa.Assign(es0t1);
    ipa.SetBase("172.16.1.0", "255.255.255.0");
    ipa.Assign(es1t2);

    // T1 to T2 bottleneck
    NetDeviceContainer t1t2dev = t1t2.Install(aggSwitches.Get(0), aggSwitches.Get(1));
    ipa.SetBase("192.168.0.0", "255.255.255.0");
    t1t2If = ipa.Assign(t1t2dev);

    // Install RED (ECN) on T1 -> T2 (bottleneck) outgoing device
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
        "MinTh", DoubleValue(65 * pktSize), 
        "MaxTh", DoubleValue(130 * pktSize), 
        "LinkBandwidth", StringValue(T1T2Rate),
        "LinkDelay", StringValue(bottleneckDelay),
        "UseEcn", BooleanValue(true));
    QueueDiscContainer qd1 = tch.Install(t1t2dev.Get(0)); // T1->T2

    // T2 to R1 bottleneck
    NetDeviceContainer t2r1dev = t2r1.Install(aggSwitches.Get(1), r1);
    ipa.SetBase("192.168.1.0", "255.255.255.0");
    t2r1If = ipa.Assign(t2r1dev);

    // Install RED (ECN) on T2 -> R1 (bottleneck) outgoing device
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
        "MinTh", DoubleValue(65 * pktSize),
        "MaxTh", DoubleValue(130 * pktSize),
        "LinkBandwidth", StringValue(T2R1Rate),
        "LinkDelay", StringValue(bottleneckDelay),
        "UseEcn", BooleanValue(true));
    QueueDiscContainer qd2 = tch.Install(t2r1dev.Get(0)); // T2->R1

    // T1, T2, R1 need to be on the Internet
    // Remaining assignment: T1's link to ES0 and T2's link to ES1 may need IPs for routing (done above)
    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ============ TCP FLOW SETUP ============
    // Each sender connects to its receiver
    ApplicationContainer sinkApps;
    std::vector<Address> sinkAddresses(numFlows);

    uint16_t basePort = 9000;

    // Install Sinks
    for (uint32_t i = 0; i < numFlows; ++i)
    {
        Address sinkAddr(InetSocketAddress(receiverIfs[i].GetAddress(0), basePort + i));
        sinkAddresses[i] = sinkAddr;
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
        ApplicationContainer app = sinkHelper.Install(receivers.Get(i));
        app.Start(Seconds(simStart));
        app.Stop(Seconds(simStop));
        sinkApps.Add(app);

        // Save for throughput measurement
        Ptr<PacketSink> sink = StaticCast<PacketSink>(app.Get(0));
        flowSinks[i] = sink;
    }

    // Each sender starts a TCP flow to its paired receiver.
    for (uint32_t i = 0; i < numFlows; ++i)
    {
        Ptr<Socket> sock = Socket::CreateSocket(senders.Get(i), TcpSocketFactory::GetTypeId());
        // Set DCTCP if available, else use TcpNewReno
        TypeId tcpTypeId = TypeId::LookupByNameFailSafe("ns3::TcpDctcp");
        if (!tcpTypeId.IsValid())
            tcpTypeId = TcpNewReno::GetTypeId();
        sock->SetAttribute("TypeId", TypeIdValue(tcpTypeId));

        Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication>();
        app->SetAttribute("Remote", AddressValue(sinkAddresses[i]));
        app->SetAttribute("MaxBytes", UintegerValue(0));
        app->SetAttribute("SendSize", UintegerValue(pktSize));
        senders.Get(i)->AddApplication(app);

        double flowStart = simStart + (startWindow * i) / numFlows;
        app->SetStartTime(Seconds(flowStart));
        app->SetStopTime(Seconds(simStop));
    }

    // ========== MONITORING ===========
    // Install FlowMonitor (for aggregate stats)
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // ========== RUN SIMULATION ===========
    Simulator::Stop(Seconds(simStop));
    Simulator::Run();

    // ========== STATISTICS OUTPUT ===========

    // Measure throughput per flow during measurement window
    std::vector<double> throughputs(numFlows);

    double t1 = measureStart;
    double t2 = measureStart + measureDuration;

    for (uint32_t i = 0; i < numFlows; ++i)
    {
        Ptr<PacketSink> sink = flowSinks[i];
        // Get total received bytes at start and end of measurement interval
        uint64_t rxStart = sink->GetTotalRx();
        Simulator::Schedule(Seconds(t2 - Simulator::Now().GetSeconds()), &Simulator::Stop, Seconds(t2));
        // (Note: Can't rewind simulation for actual interval, so approximate using total Rx in window.)
        double bits = sink->GetTotalRx() * 8.0;
        double throughput = bits / (t2 - simStart) / 1e9; // in Gbps
        throughputs[i] = throughput;
    }

    // Output per-flow throughput
    std::ofstream outThroughput("per-flow-throughput.txt");
    for (uint32_t i = 0; i < numFlows; ++i)
    {
        outThroughput << "Flow " << i << ": " << throughputs[i] << " Gbps" << std::endl;
    }
    outThroughput.close();

    // Calculate and output fairness index
    double fairness = CalculateFairness(throughputs);
    std::ofstream outFairness("aggregate-fairness.txt");
    outFairness << "Jain's Fairness index: " << fairness << std::endl;
    outFairness.close();

    // Output RED queue stats for both bottlenecks
    std::ofstream outQueues("queue-stats.txt");
    Ptr<QueueDisc> qRed1 = qd1.Get(0);
    Ptr<QueueDisc> qRed2 = qd2.Get(0);
    outQueues << "T1-T2 RED/ECN queue average: " << qRed1->GetQueueDiscSize().GetValue() << " packets" << std::endl;
    outQueues << "T2-R1 RED/ECN queue average: " << qRed2->GetQueueDiscSize().GetValue() << " packets" << std::endl;
    outQueues.close();

    // Output FlowMonitor summary
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::ofstream outMonitor("flow-monitor.txt");
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        outMonitor << "FlowID: " << flow.first << " Src: " << t.sourceAddress << " Dst: " << t.destinationAddress <<
            " Throughput: " << flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e9 << " Gbps" << std::endl;
    }
    outMonitor.close();

    Simulator::Destroy();
    return 0;
}