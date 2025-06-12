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
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpNewReno")));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", DoubleValue(5),
                         "MaxTh", DoubleValue(15),
                         "LinkBandwidth", DataRateValue(DataRate("5Mbps")),
                         "LinkDelay", TimeValue(MilliSeconds(2)));
    QueueDiscContainer qdiscs = tch.Install(devices);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("Remote", AddressValue(sinkAddress));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer sourceApps = onOffHelper.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(9.0));

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("red-queue-disc.tr");
    pointToPoint.EnableAsciiAll(stream);
    pointToPoint.EnablePcapAll("red-queue-disc");

    Simulator::Schedule(Seconds(0.1), []() {
        for (uint32_t i = 0; i < qdiscs.GetN(); ++i) {
            Ptr<RedQueueDisc> redQd = DynamicCast<RedQueueDisc>(qdiscs.Get(i));
            if (redQd) {
                std::cout << "RED Queue Disc " << i << ": "
                          << "Current queue size: " << redQd->GetCurrentLevel() << " packets, "
                          << "Dropped packets: " << redQd->GetStats().nTotalDrops << std::endl;
            }
        }
        Simulator::Schedule(Seconds(1.0), &[](void){});
    });

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s\n";
        std::cout << "  Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s\n";
    }

    Simulator::Destroy();
    return 0;
}