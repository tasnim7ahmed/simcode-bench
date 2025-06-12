#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    std::string queueDiscType = "PfifoFast";
    uint64_t bottleneckBandwidth = 10000000; // Default 10 Mbps
    Time bottleneckDelay = MilliSeconds(10);
    bool enableBql = false;
    double simulationTime = 60.0;

    CommandLine cmd;
    cmd.AddValue("queueDiscType", "Queue disc type to use: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueDiscType);
    cmd.AddValue("bottleneckBandwidth", "Bottleneck bandwidth in bps", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer n1n2;
    n1n2.Create(2);
    NodeContainer n2n3;
    n2n3.Add(n1n2.Get(1));
    n2n3.Create(1);

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("0.1ms"));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bottleneckBandwidth)));
    p2p_n2n3.SetChannelAttribute("Delay", TimeValue(bottleneckDelay));

    if (enableBql) {
        p2p_n2n3.SetQueue("ns3::DynamicQueueLimits");
    }

    NetDeviceContainer dev_n1n2 = p2p_n1n2.Install(n1n2);
    NetDeviceContainer dev_n2n3 = p2p_n2n3.Install(n2n3);

    TrafficControlHelper tch;
    if (queueDiscType == "ARED") {
        tch.SetRootQueueDisc("ns3::AredQueueDisc");
    } else if (queueDiscType == "CoDel") {
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
    } else if (queueDiscType == "FqCoDel") {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    } else if (queueDiscType == "PIE") {
        tch.SetRootQueueDisc("ns3::PieQueueDisc");
    } else {
        tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    }

    QueueDiscContainer qdiscs = tch.Install(dev_n2n3);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n1n2 = address.Assign(dev_n1n2);
    address.NewNetwork();
    Ipv4InterfaceContainer i_n2n3 = address.Assign(dev_n2n3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(i_n2n3.GetAddress(1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Gbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1448));
    onoff.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientApp = onoff.Install(n1n2.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(n2n3.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    OnOffHelper onoffReverse("ns3::TcpSocketFactory", InetSocketAddress(i_n1n2.GetAddress(0), port + 1));
    onoffReverse.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffReverse.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffReverse.SetAttribute("DataRate", StringValue("1Gbps"));
    onoffReverse.SetAttribute("PacketSize", UintegerValue(1448));
    onoffReverse.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer clientAppReverse = onoffReverse.Install(n2n3.Get(1));
    clientAppReverse.Start(Seconds(0.0));
    clientAppReverse.Stop(Seconds(simulationTime));

    PacketSinkHelper sinkReverse("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer serverAppReverse = sinkReverse.Install(n1n2.Get(0));
    serverAppReverse.Start(Seconds(0.0));
    serverAppReverse.Stop(Seconds(simulationTime));

    V4PingHelper ping(i_n1n2.GetAddress(0));
    ping.SetAttribute("Remote", Ipv4AddressValue(i_n2n3.GetAddress(1)));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer pingApp = ping.Install(n2n3.Get(1));
    pingApp.Start(Seconds(0.0));
    pingApp.Stop(Seconds(simulationTime));

    AsciiTraceHelper ascii;
    p2p_n2n3.EnableAsciiAll(ascii.CreateFileStream("queue-disc-comparison.tr"));
    p2p_n2n3.EnablePcapAll("queue-disc-comparison");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier().Peek())->FindFlow(iter->first);
        if (t.sourcePort == port || t.sourcePort == port + 1) {
            std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
            std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
            std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps" << std::endl;
            std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}