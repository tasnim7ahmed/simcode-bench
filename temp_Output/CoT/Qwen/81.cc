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

    Time::SetResolution(Time::NS);
    LogComponentEnable("RedQueueDisc", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", DoubleValue(5),
                         "MaxTh", DoubleValue(15),
                         "LinkBandwidth", DataRateValue(DataRate("10Mbps")),
                         "LinkDelay", TimeValue(MilliSeconds(10)));
    QueueDiscContainer qdiscs = tch.Install(devices);

    // Enable queue and sojourn time tracing
    for (uint32_t i = 0; i < qdiscs.GetN(); ++i) {
        Ptr<QueueDisc> q = qdiscs.Get(i);
        q->TraceConnectWithoutContext("Enqueue", MakeCallback([](Ptr<const Packet> p) {
            NS_LOG_INFO(Simulator::Now().GetSeconds() << " Enqueued packet at traffic control layer");
        }));
        q->TraceConnectWithoutContext("Dequeue", MakeCallback([q](Ptr<const Packet> p, const Address &) {
            double delay = (Simulator::Now() - p->GetTimestamp()).GetSeconds();
            NS_LOG_INFO(Simulator::Now().GetSeconds() << " Dequeued packet at traffic control layer with sojourn time: " << delay << "s");
        }));
        q->TraceConnectWithoutContext("Drop", MakeCallback([](Ptr<const Packet> p) {
            NS_LOG_INFO(Simulator::Now().GetSeconds() << " Dropped packet at traffic control layer");
        }));
    }

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback([](Ptr<const Packet> p) {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << " Enqueued packet at device layer");
    }));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Dequeue", MakeCallback([](Ptr<const Packet> p) {
        double delay = (Simulator::Now() - p->GetTimestamp()).GetSeconds();
        NS_LOG_INFO(Simulator::Now().GetSeconds() << " Dequeued packet at device layer with sojourn time: " << delay << "s");
    }));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Drop", MakeCallback([](Ptr<const Packet> p) {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << " Dropped packet at device layer");
    }));

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = bulkSend.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    std::map<FlowId, FlowStats> stats = flowMonitor->GetFlowStats();

    for (std::map<FlowId, FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.sourcePort == sinkPort || t.destinationPort == sinkPort) {
            std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
            std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
            std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
            std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s\n";
            std::cout << "  Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s\n";
        }
    }

    Simulator::Destroy();
    return 0;
}