#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpBottleneckSimulation");

int main(int argc, char *argv[]) {
    uint32_t numFlows = 40;
    double startTime = 0.0;
    double convergenceTime = 3.0;
    double measurementDuration = 1.0;
    double stopTime = startTime + convergenceTime + measurementDuration;
    std::string transportProtocol = "TcpDctcp";

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(transportProtocol)));
    Config::SetDefault("ns3::RedQueueDisc::Mode", EnumValue(RedQueueDisc::QUEUE_DISC_MODE_BYTES));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(500 * 600));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(1500 * 600));
    Config::SetDefault("ns3::RedQueueDisc::QueueLimit", UintegerValue(2000 * 600));
    Config::SetDefault("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", EnumValue(TcpSocketBase::On));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));

    NodeContainer senders1, senders2, t1, t2, r1;
    senders1.Create(30);
    senders2.Create(20);
    t1.Create(1);
    t2.Create(1);
    r1.Create(1);

    PointToPointHelper p2pT1T2;
    p2pT1T2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2pT1T2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    p2pT1T2.SetQueue("ns3::RedQueueDisc");

    PointToPointHelper p2pT2R1;
    p2pT2R1.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
    p2pT2R1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    p2pT2R1.SetQueue("ns3::RedQueueDisc");

    PointToPointHelper p2pHostSwitch;
    p2pHostSwitch.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
    p2pHostSwitch.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devSender1ToT1, devSender2ToT2;
    for (uint32_t i = 0; i < senders1.GetN(); ++i) {
        devSender1ToT1.Add(p2pHostSwitch.Install(senders1.Get(i), t1.Get(0)));
    }
    for (uint32_t i = 0; i < senders2.GetN(); ++i) {
        devSender2ToT2.Add(p2pHostSwitch.Install(senders2.Get(i), t2.Get(0)));
    }

    NetDeviceContainer devT1T2 = p2pT1T2.Install(t1.Get(0), t2.Get(0));
    NetDeviceContainer devT2R1 = p2pT2R1.Install(t2.Get(0), r1.Get(0));

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifSender1ToT1 = ipv4.Assign(devSender1ToT1);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifSender2ToT2 = ipv4.Assign(devSender2ToT2);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifT1T2 = ipv4.Assign(devT1T2);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifT2R1 = ipv4.Assign(devT2R1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(r1.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(stopTime));

    std::vector<ApplicationContainer> appContainers(numFlows);
    for (uint32_t i = 0; i < numFlows; ++i) {
        Ptr<Node> senderNode;
        if (i < 30) {
            senderNode = senders1.Get(i);
        } else {
            senderNode = senders2.Get(i - 30);
        }

        InetSocketAddress sinkAddress(ifT2R1.GetAddress(1), port);
        sinkAddress.SetTos(0x10);
        OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
        onoff.SetConstantRate(DataRate("10Gbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1448));

        double flowStart = Seconds(startTime + (double)i / numFlows);
        ApplicationContainer apps = onoff.Install(senderNode);
        apps.Start(Seconds(flowStart));
        apps.Stop(Seconds(stopTime));
        appContainers[i] = apps;
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    std::cout.precision(6);
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        if (i->first == 1) continue;

        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::ostringstream protoStream;
        protoStream << (uint32_t)t.protocol;
        std::string proto = protoStream.str();

        if (t.sourcePort != port && t.destinationPort != port) continue;

        Time endEvent = i->second.timeLastRxPacket;
        if ((endEvent > Seconds(convergenceTime)) && (endEvent <= Seconds(convergenceTime + measurementDuration))) {
            double throughput = i->second.rxBytes * 8.0 / measurementDuration / 1e9;
            totalThroughput += throughput;
            flowCount++;
            std::cout << "Flow " << i->first
                      << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")"
                      << " throughput: " << throughput << " Gbps" << std::endl;
        }
    }

    if (flowCount > 0) {
        double avgThroughput = totalThroughput / flowCount;
        double fairnessNumerator = 0.0;
        double fairnessDenominator = 0.0;
        for (auto const& kv : stats) {
            if (kv.first == 1) continue;
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(kv.first);
            if (t.sourcePort != port && t.destinationPort != port) continue;
            Time endEvent = kv.second.timeLastRxPacket;
            if ((endEvent > Seconds(convergenceTime)) && (endEvent <= Seconds(convergenceTime + measurementDuration))) {
                double x_i = kv.second.rxBytes * 8.0 / measurementDuration / 1e9;
                fairnessNumerator += x_i;
                fairnessDenominator += x_i * x_i;
            }
        }
        fairnessNumerator = fairnessNumerator * fairnessNumerator;
        double aggregateFairness = fairnessNumerator / (flowCount * fairnessDenominator);
        std::cout << "Aggregate Fairness Index: " << aggregateFairness << std::endl;
    }

    std::cout << "Total Throughput: " << totalThroughput << " Gbps over " << flowCount << " flows" << std::endl;

    Simulator::Destroy();
    return 0;
}