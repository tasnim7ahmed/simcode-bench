#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpFig17Simulation");

// Globals for statistics collection
std::vector<double> flowThroughputs(40, 0.0);
std::map<uint32_t, ApplicationContainer> appMap;
Time measureStart = Seconds(3.0);
Time measureDuration = Seconds(1.0);

void
TraceQueueStats(Ptr<QueueDisc> queue, std::string name)
{
    uint32_t packetsInQueue = queue->GetCurrentSize().GetValue();
    uint32_t bytesInQueue = queue->GetCurrentSize().GetUnit() == QueueSizeUnit::BYTES ? queue->GetCurrentSize().GetValue() : 0;
    NS_LOG_UNCOND("["<< Simulator::Now().GetSeconds() << "s] " << name << " queue Occupancy: " << packetsInQueue << " packets, " << bytesInQueue << " bytes");
}

void
ScheduleQueueStatTraces(Ptr<QueueDisc> queue, std::string name)
{
    Simulator::Schedule(Seconds(0.01), [=]() {
        TraceQueueStats(queue, name);
        ScheduleQueueStatTraces(queue, name);
    });
}

static void
FlowMonitorThroughput(Ptr<FlowMonitor> monitor,
                      FlowMonitorHelper &flowHelper,
                      double startTime,
                      double duration)
{
    // Per flow stats
    double fairnessIndex = 0.0;
    std::vector<double> throughputs(40, 0.0);
    double sumThroughput = 0.0, sumThroughput2 = 0.0;

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        uint32_t flowIndex = 0xffffffff;

        // All senders use port 5000+flowid
        if (t.sourcePort >= 5000 && t.sourcePort < 5040)
            flowIndex = t.sourcePort - 5000;
        else if (t.destinationPort >= 5000 && t.destinationPort < 5040)
            flowIndex = t.destinationPort - 5000;
        else
            continue;

        // Find intersection with measurement window
        double flowStart = std::max(startTime, it->second.timeFirstTxPacket.GetSeconds());
        double flowEnd   = std::min(startTime+duration, it->second.timeLastRxPacket.GetSeconds());
        if (flowEnd <= flowStart) continue;

        double rxBytes = 0;
        if (it->second.timeLastRxPacket.GetSeconds() >= (startTime+duration))
        {
            double totInterval = it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds();
            if (totInterval > 0)
            {
                double measureInterval = duration;
                rxBytes = (double)it->second.rxBytes * (measureInterval / totInterval);
            }
            else
                rxBytes = it->second.rxBytes;
        }
        else
            rxBytes = it->second.rxBytes;

        double throughput = (rxBytes*8.0) / (1e9 * duration); // Gbps
        throughputs[flowIndex] = throughput;
        sumThroughput += throughput;
        sumThroughput2 += throughput*throughput;
    }

    // Per-flow throughput output
    std::ofstream flowOut("per-flow-throughput.txt");
    for (uint32_t i=0; i< throughputs.size(); ++i)
    {
        flowOut << "Flow " << i << ": " << throughputs[i] << " Gbps" << std::endl;
    }
    flowOut.close();

    // Fairness index (Jain's)
    if (sumThroughput2 > 0)
        fairnessIndex = (sumThroughput*sumThroughput) / (throughputs.size()*sumThroughput2);

    std::ofstream aggr("aggregate-fairness.txt");
    aggr << "Aggregate throughput: " << sumThroughput << " Gbps" << std::endl;
    aggr << "Jain's Fairness Index: " << fairnessIndex << std::endl;
    aggr.close();
}

int main(int argc, char *argv[])
{
    LogComponentEnable("DctcpFig17Simulation", LOG_LEVEL_INFO);

    uint32_t nSenders = 40;
    uint32_t nReceivers = 40;
    uint32_t nT1 = 2, nT2 = 2, nR1 = 1, nR2 = 1; // switches, receivers
    double bottleneckBps1 = 10e9; // 10 Gbps (T1-T2)
    double bottleneckBps2 = 1e9;  // 1 Gbps (T2-R1)
    std::string bottleneck1Delay = "50us";
    std::string bottleneck2Delay = "50us";
    std::string leafDelay = "10us";
    uint32_t mtuBytes = 1500;
    uint32_t queueSizePackets = 1000;

    // Topology:
    // [S0-S29]--T1--T2--R1--[D0-D19]
    // [S30-S39]--T2--R2--[D20-D39]
    // T1,T2,R1,R2 are switches/routers

    NodeContainer senders;
    senders.Create(nSenders);
    NodeContainer receivers;
    receivers.Create(nReceivers);
    NodeContainer t1; t1.Create(nT1);
    NodeContainer t2; t2.Create(nT2);
    NodeContainer r1; r1.Create(nR1);
    NodeContainer r2; r2.Create(nR2);

    // All P2P links
    PointToPointHelper p2pLeaf;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue("40Gbps"));
    p2pLeaf.SetChannelAttribute("Delay", StringValue(leafDelay));
    p2pLeaf.SetDeviceAttribute("Mtu", UintegerValue(mtuBytes));
    p2pLeaf.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queueSizePackets));

    PointToPointHelper p2pT1T2;
    p2pT1T2.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bottleneckBps1)));
    p2pT1T2.SetChannelAttribute("Delay", StringValue(bottleneck1Delay));
    p2pT1T2.SetDeviceAttribute("Mtu", UintegerValue(mtuBytes));
    // No drop-tail Q: will use RED via TrafficControl

    PointToPointHelper p2pT2R1;
    p2pT2R1.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bottleneckBps2)));
    p2pT2R1.SetChannelAttribute("Delay", StringValue(bottleneck2Delay));
    p2pT2R1.SetDeviceAttribute("Mtu", UintegerValue(mtuBytes));

    PointToPointHelper p2pT2R2;
    p2pT2R2.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bottleneckBps1)));
    p2pT2R2.SetChannelAttribute("Delay", StringValue(bottleneck1Delay));
    p2pT2R2.SetDeviceAttribute("Mtu", UintegerValue(mtuBytes));

    // Devices and node connections
    // S0-29 <-> T1[0];  S30-39 <-> T2[1]

    std::vector<NetDeviceContainer> senderDevsList;
    for (uint32_t i = 0; i < 30; ++i)
        senderDevsList.push_back(p2pLeaf.Install(senders.Get(i), t1.Get(0)));
    for (uint32_t i = 30; i < 40; ++i)
        senderDevsList.push_back(p2pLeaf.Install(senders.Get(i), t2.Get(1)));

    // T1[0] <-> T2[0] is 10Gbps bottleneck
    NetDeviceContainer t1t2Link = p2pT1T2.Install(t1.Get(0), t2.Get(0));

    // T2[0] <-> R1[0] is 1Gbps bottleneck
    NetDeviceContainer t2r1Link = p2pT2R1.Install(t2.Get(0), r1.Get(0));

    // R1[0] <-> D0-D19 (receivers 0-19)
    std::vector<NetDeviceContainer> r1recvDevsList;
    for (uint32_t i=0; i<20; ++i)
        r1recvDevsList.push_back(p2pLeaf.Install(r1.Get(0), receivers.Get(i)));

    // T2[1] <-> R2[0] (for senders 30-39 to receivers 20-39)
    NetDeviceContainer t2r2Link = p2pT2R2.Install(t2.Get(1), r2.Get(0));

    // R2[0] <-> D20-D39
    std::vector<NetDeviceContainer> r2recvDevsList;
    for (uint32_t i=20; i<40; ++i)
        r2recvDevsList.push_back(p2pLeaf.Install(r2.Get(0), receivers.Get(i)));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(receivers);
    stack.Install(t1);
    stack.Install(t2);
    stack.Install(r1);
    stack.Install(r2);

    // Assign address space
    Ipv4AddressHelper addr;
    std::vector<Ipv4InterfaceContainer> senderIfaces;
    for (uint32_t i = 0; i < 30; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        addr.SetBase(subnet.str().c_str(), "255.255.255.0");
        senderIfaces.push_back(addr.Assign(senderDevsList[i])); // S0-29
    }
    for (uint32_t i = 30; i < 40; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << (i-30) << ".0";
        addr.SetBase(subnet.str().c_str(), "255.255.255.0");
        senderIfaces.push_back(addr.Assign(senderDevsList[i]));  // S30-39
    }

    addr.SetBase("10.10.0.0", "255.255.255.0");
    Ipv4InterfaceContainer t1t2Ifaces = addr.Assign(t1t2Link);

    addr.SetBase("10.20.0.0", "255.255.255.0");
    Ipv4InterfaceContainer t2r1Ifaces = addr.Assign(t2r1Link);

    for (uint32_t i=0; i<20; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.31." << i << ".0";
        addr.SetBase(subnet.str().c_str(), "255.255.255.0");
        addr.Assign(r1recvDevsList[i]);
    }

    addr.SetBase("10.30.0.0", "255.255.255.0");
    Ipv4InterfaceContainer t2r2Ifaces = addr.Assign(t2r2Link);

    for (uint32_t i=20; i<40; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.41." << i << ".0";
        addr.SetBase(subnet.str().c_str(), "255.255.255.0");
        addr.Assign(r2recvDevsList[i-20]);
    }

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure RED with ECN for T1-T2 and T2-R1 links
    TrafficControlHelper tch;
    uint32_t redHandle = tch.SetRootQueueDisc("ns3::RedQueueDisc",
        "MaxSize", QueueSizeValue(QueueSize("1000p")),
        "MinTh", DoubleValue(30 * mtuBytes),
        "MaxTh", DoubleValue(90 * mtuBytes),
        "LinkBandwidth", StringValue("10Gbps"),
        "LinkDelay", StringValue(bottleneck1Delay),
        "MeanPktSize", UintegerValue(mtuBytes),
        "QW", DoubleValue(0.002),
        "UseEcn", BooleanValue(true)
    );
    // T1-T2 Red + ECN on T1's outgoing device
    Ptr<NetDevice> t1DevOut = t1t2Link.Get(0);
    Ptr<Node> t1Node = t1.Get(0);
    tch.Install(t1DevOut);
    // Schedule queue monitoring for RED queue
    Ptr<QueueDisc> t1QueueDisc = t1Node->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(t1DevOut);
    ScheduleQueueStatTraces(t1QueueDisc, "T1-T2 RED");

    // T2-R1 Red + ECN on T2's outgoing device
    TrafficControlHelper tch2;
    tch2.SetRootQueueDisc("ns3::RedQueueDisc",
        "MaxSize", QueueSizeValue(QueueSize("1000p")),
        "MinTh", DoubleValue(30 * mtuBytes),
        "MaxTh", DoubleValue(90 * mtuBytes),
        "LinkBandwidth", StringValue("1Gbps"),
        "LinkDelay", StringValue(bottleneck2Delay),
        "MeanPktSize", UintegerValue(mtuBytes),
        "QW", DoubleValue(0.002),
        "UseEcn", BooleanValue(true)
    );
    Ptr<NetDevice> t2DevOut = t2r1Link.Get(0);
    Ptr<Node> t2Node_0 = t2.Get(0);
    tch2.Install(t2DevOut);
    Ptr<QueueDisc> t2QueueDisc = t2Node_0->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(t2DevOut);
    ScheduleQueueStatTraces(t2QueueDisc, "T2-R1 RED");

    // Use DCTCP TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));

    // Setup TCP flows
    ApplicationContainer serverApps, clientApps;

    // flows 0-29: S0-29 -> D0-19 (via R1)
    // There are exactly 20 unique receiver endpoints via R1
    for (uint32_t i = 0; i < 30; ++i)
    {
        uint32_t recvIdx = i % 20;
        Address sinkLocalAddr(InetSocketAddress(Ipv4Address::GetAny(), 5000+i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddr);
        ApplicationContainer sinkApp = sinkHelper.Install(receivers.Get(recvIdx));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
        serverApps.Add(sinkApp);
        appMap[i] = sinkApp;

        // Start times: uniformly across 1s
        double flowStart = 1.0 * i / 40.0;
        BulkSendHelper sender("ns3::TcpSocketFactory", InetSocketAddress(receivers.Get(recvIdx)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 5000+i));
        sender.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
        ApplicationContainer sourceApp = sender.Install(senders.Get(i));
        sourceApp.Start(Seconds(flowStart));
        sourceApp.Stop(Seconds(10.0));
        clientApps.Add(sourceApp);
    }

    // flows 30-39: S30-39 -> D20-39 (via R2)
    for (uint32_t i = 30; i < 40; ++i)
    {
        uint32_t recvIdx = i;
        Address sinkLocalAddr(InetSocketAddress(Ipv4Address::GetAny(), 5000+i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddr);
        ApplicationContainer sinkApp = sinkHelper.Install(receivers.Get(recvIdx));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
        serverApps.Add(sinkApp);
        appMap[i] = sinkApp;

        double flowStart = 1.0 * i / 40.0;
        BulkSendHelper sender("ns3::TcpSocketFactory", InetSocketAddress(receivers.Get(recvIdx)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 5000+i));
        sender.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer sourceApp = sender.Install(senders.Get(i));
        sourceApp.Start(Seconds(flowStart));
        sourceApp.Stop(Seconds(10.0));
        clientApps.Add(sourceApp);
    }

    // Setup flow monitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Schedule(measureStart + measureDuration + Seconds(0.3), [&] {
        FlowMonitorThroughput(monitor, flowHelper, measureStart.GetSeconds(), measureDuration.GetSeconds());
        Simulator::Stop();
    });

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}