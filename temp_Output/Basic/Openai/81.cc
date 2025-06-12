#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RedQueueDiscExample");

void PrintQueueStats(Ptr<QueueDisc> qdisc, Ptr<Queue<Packet>> deviceQueue)
{
    uint32_t qdiscLen = qdisc->GetCurrentSize().GetValue();
    uint32_t deviceLen = 0;
    if (deviceQueue)
    {
        deviceLen = deviceQueue->GetNPackets();
    }
    Simulator::Schedule(Seconds(0.1), &PrintQueueStats, qdisc, deviceQueue);

    std::cout << Simulator::Now().GetSeconds() << "s "
              << "QueueDisc packets: " << qdiscLen
              << ", Device queue packets: " << deviceLen
              << std::endl;
}

void SojournTimeTrace(double soj)
{
    static double total = 0;
    static uint32_t count = 0;
    total += soj;
    ++count;
    std::cout << Simulator::Now().GetSeconds() << "s "
              << "Sojourn time: " << soj * 1e3 << " ms, Avg: " << (total / count) * 1e3 << " ms" << std::endl;
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("RedQueueDiscExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::RedQueueDisc");
    tch.AddInternalQueues(handle, 1, "ns3::DropTailQueue", "MaxPackets", UintegerValue(100));
    QueueDiscContainer qdiscs = tch.Install(devices);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("10Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1000));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(19.0)));
    onoff.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));

    // Trace queue lengths
    Ptr<QueueDisc> qdisc = qdiscs.Get(0);
    Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(devices.Get(0));
    Ptr<Queue<Packet>> deviceQueue = dev->GetQueue();

    Simulator::Schedule(Seconds(0.1), &PrintQueueStats, qdisc, deviceQueue);

    // Trace sojourn time statistics
    qdisc->TraceConnectWithoutContext("SojournTime", MakeCallback(&SojournTimeTrace));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Output FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort != port)
            continue;
        double rxPackets = flow.second.rxPackets;
        double txPackets = flow.second.txPackets;
        double duration = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
        double throughput = duration > 0 ? ((flow.second.rxBytes * 8.0) / (duration * 1000000.0)) : 0;
        double meanDelay = txPackets > 0 ? (flow.second.delaySum.GetSeconds() / txPackets) * 1e3 : 0.0;
        double meanJitter = rxPackets > 0 ? (flow.second.jitterSum.GetSeconds() / rxPackets) * 1e3 : 0.0;

        std::cout << "\nFlowId: " << flow.first
                  << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                  << t.destinationAddress << ":" << t.destinationPort << ")\n";
        std::cout << "  Tx Packets: " << txPackets << "\n";
        std::cout << "  Rx Packets: " << rxPackets << "\n";
        std::cout << "  Throughput: " << throughput << " Mbps\n";
        std::cout << "  Mean Delay: " << meanDelay << " ms\n";
        std::cout << "  Mean Jitter: " << meanJitter << " ms\n";
    }

    Simulator::Destroy();
    return 0;
}