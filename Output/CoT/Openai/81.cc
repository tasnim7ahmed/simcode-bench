#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RedQueueDiscExample");

void
PrintQueueLengths(Ptr<QueueDisc> qdisc, Ptr<NetDevice> dev)
{
    uint32_t tcLen = qdisc->GetCurrentSize().GetValue();
    uint32_t devLen = DynamicCast<PointToPointNetDevice>(dev)->GetQueue()->GetNPackets();
    Simulator::Schedule(Seconds(0.1), &PrintQueueLengths, qdisc, dev);
    std::cout << Simulator::Now().GetSeconds() << "s "
              << "TrafficControl Queue Length: " << tcLen << " packets; "
              << "Device Queue Length: " << devLen << " packets" << std::endl;
}

class SojournTracer
{
  public:
    void EnqueueCb(Ptr<const QueueDiscItem> item)
    {
        m_enqueueTimes[item->GetPacket()->GetUid()] = Simulator::Now();
    }

    void DequeueCb(Ptr<const QueueDiscItem> item)
    {
        auto it = m_enqueueTimes.find(item->GetPacket()->GetUid());
        if (it != m_enqueueTimes.end())
        {
            Time sojourn = Simulator::Now() - it->second;
            m_sojournTimes.push_back(sojourn.GetSeconds());
            m_enqueueTimes.erase(it);
        }
    }

    void PrintStats()
    {
        if (m_sojournTimes.empty())
        {
            std::cout << "No packets departed - no sojourn times tracked." << std::endl;
            return;
        }
        double sum = 0.0, sqsum = 0.0;
        for (double t : m_sojournTimes) {
            sum += t; sqsum += t * t;
        }
        double mean = sum / m_sojournTimes.size();
        double var = (sqsum / m_sojournTimes.size()) - (mean * mean);
        std::cout << "Packet Sojourn Time (mean): " << mean << " s, stddev: " << std::sqrt(var) << " s" << std::endl;
    }

  private:
    std::map<uint64_t, Time> m_enqueueTimes;
    std::vector<double> m_sojournTimes;
};

int main(int argc, char *argv[])
{
    // Set simulation parameters
    double simTime = 10.0; // seconds
    std::string bottleneckBandwidth = "10Mbps";
    std::string bottleneckDelay = "5ms";
    uint32_t maxPackets = 100;

    // Create nodes and devices
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    p2p.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(maxPackets));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install RedQueueDisc on the outbound device of Node 0
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::RedQueueDisc", "MinTh", DoubleValue(5), "MaxTh", DoubleValue(15));
    QueueDiscContainer qdiscs = tch.Install(devices.Get(0));

    // Setup sojourn time tracing object
    Ptr<QueueDisc> qdisc = qdiscs.Get(0);
    SojournTracer tracer;
    qdisc->TraceConnectWithoutContext("Enqueue", MakeCallback(&SojournTracer::EnqueueCb, &tracer));
    qdisc->TraceConnectWithoutContext("Dequeue", MakeCallback(&SojournTracer::DequeueCb, &tracer));

    // Schedule recurring queue length printout
    Simulator::Schedule(Seconds(0.1), &PrintQueueLengths, qdiscs.Get(0), devices.Get(0));

    // Set up TCP application: BulkSend (sender at node 0), PacketSink (receiver at node 1)
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    bulkSender.SetAttribute("SendSize", UintegerValue(1448));
    ApplicationContainer sourceApps = bulkSender.Install(nodes.Get(0));
    sourceApps.Start(Seconds(0.1));
    sourceApps.Stop(Seconds(simTime));

    // Enable FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable PCAP tracing if needed
    p2p.EnablePcapAll("redq-example", true);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Print Sojourn stats
    tracer.PrintStats();

    // Print FlowMonitor stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        double timeFirstTx = flow.second.timeFirstTxPacket.GetSeconds();
        double timeLastRx = flow.second.timeLastRxPacket.GetSeconds();
        double duration = timeLastRx - timeFirstTx;
        uint64_t rxBytes = flow.second.rxBytes;
        double throughput = duration > 0 ? (rxBytes * 8.0 / duration) / 1e6 : 0.0;
        std::cout << "  Throughput: " << throughput << " Mbps\n";
        if (flow.second.rxPackets > 0)
        {
            double meanDelay = 1.0 * flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            double meanJitter = 1.0 * flow.second.jitterSum.GetSeconds() / flow.second.rxPackets;
            std::cout << "  Mean Delay: " << meanDelay << " s\n";
            std::cout << "  Mean Jitter: " << meanJitter << " s\n";
        }
        else
        {
            std::cout << "  No packets received.\n";
        }
    }

    Simulator::Destroy();
    return 0;
}