#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RedQueueDiscExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", DoubleValue(5),
                         "MaxTh", DoubleValue(15),
                         "LinkBandwidth", DataRateValue(DataRate("10Mbps")),
                         "LinkDelay", TimeValue(MilliSeconds(10)));
    QueueDiscContainer qdiscs = tch.Install(devices);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("8Mbps"), 500);
    ApplicationContainer sourceApps = onoff.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(9.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("red-queue-disc.tr");
    pointToPoint.EnableAsciiAll(stream);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print queue statistics
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<NetDevice> device = devices.Get(i);
        Ptr<PointToPointNetDevice> p2pDevice = DynamicCast<PointToPointNetDevice>(device);
        if (p2pDevice) {
            Ptr<Queue<Packet>> deviceQueue = p2pDevice->GetQueue();
            std::cout << "Device Queue Size (" << i << "): " << deviceQueue->GetNPackets() << " packets" << std::endl;
        }

        Ptr<QueueDisc> qdisc = qdiscs.Get(i);
        std::cout << "Traffic Control Queue Size (" << i << "): " << qdisc->GetNPacketsInQueue() << " packets" << std::endl;
        std::cout << "Sojourn Time (" << i << "): " << qdisc->GetMeanSojournTime().As(Time::MS) << std::endl;
    }

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "\nFlow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s\n";
        std::cout << "  Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}