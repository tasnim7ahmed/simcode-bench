#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    int nLeafNodes = 10;
    std::string queueType = "RED";
    std::string queueDiscLimit = "1000p";
    uint32_t packetSize = 1024;
    std::string dataRate = "10Mbps";
    std::string delay = "2ms";
    std::string redMeanPktSize = "1000";
    double redWeight = 0.002;
    std::string redMinTh = "5p";
    std::string redMaxTh = "15p";
    bool byteMode = false;
    bool enablePcap = false;

    cmd.AddValue("nLeafNodes", "Number of leaf nodes", nLeafNodes);
    cmd.AddValue("queueType", "Queue type (RED or NLRED)", queueType);
    cmd.AddValue("queueDiscLimit", "Queue disc limit", queueDiscLimit);
    cmd.AddValue("packetSize", "Packet size", packetSize);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("delay", "Delay", delay);
    cmd.AddValue("redMeanPktSize", "RED mean packet size", redMeanPktSize);
    cmd.AddValue("redWeight", "RED weight", redWeight);
    cmd.AddValue("redMinTh", "RED minimum threshold", redMinTh);
    cmd.AddValue("redMaxTh", "RED maximum threshold", redMaxTh);
    cmd.AddValue("byteMode", "Enable byte mode", byteMode);
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    NodeContainer leftRouters, rightRouters, leafNodes;
    leftRouters.Create(1);
    rightRouters.Create(1);
    leafNodes.Create(2 * nLeafNodes);

    NodeContainer leftLeafNodes, rightLeafNodes;
    for (int i = 0; i < nLeafNodes; ++i) {
        leftLeafNodes.Add(leafNodes.Get(i));
        rightLeafNodes.Add(leafNodes.Get(i + nLeafNodes));
    }

    InternetStackHelper stack;
    stack.Install(leftRouters);
    stack.Install(rightRouters);
    stack.Install(leafNodes);

    PointToPointHelper routerLink;
    routerLink.DataRate(dataRate);
    routerLink.Delay(delay);

    PointToPointHelper leafLink;
    leafLink.DataRate(dataRate);
    leafLink.Delay(delay);

    NetDeviceContainer leftRouterDevices, rightRouterDevices;
    leftRouterDevices = leafLink.Install(leftRouters, leftLeafNodes);
    rightRouterDevices = leafLink.Install(rightRouters, rightLeafNodes);

    NetDeviceContainer centerLinkDevices = routerLink.Install(leftRouters, rightRouters);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftRouterInterfaces, rightRouterInterfaces;
    leftRouterInterfaces = address.Assign(leftRouterDevices);
    rightRouterInterfaces = address.Assign(rightRouterDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer centerLinkInterfaces = address.Assign(centerLinkDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure Queue Disc
    TypeId queueTypeId;
    if (queueType == "RED") {
        queueTypeId = TypeId::LookupByName("ns3::RedQueueDisc");
    } else if (queueType == "NLRED") {
        queueTypeId = TypeId::LookupByName("ns3::NlRedQueueDisc");
    } else {
        std::cerr << "Invalid queue type.  Must be RED or NLRED." << std::endl;
        return 1;
    }

    QueueDiscContainer queueDiscs;
    QueueDiscHelper queueDiscHelper(queueType);
    queueDiscHelper.Set("LinkBandwidth", StringValue(dataRate));
    queueDiscHelper.Set("queueLimit", StringValue(queueDiscLimit));
    if (queueType == "RED" || queueType == "NLRED") {
        queueDiscHelper.Set("MeanPktSize", StringValue(redMeanPktSize));
        queueDiscHelper.Set("Weight", DoubleValue(redWeight));
        queueDiscHelper.Set("MinTh", StringValue(redMinTh));
        queueDiscHelper.Set("MaxTh", StringValue(redMaxTh));
        queueDiscHelper.Set("Bytes", BooleanValue(byteMode));
    }

    NetDeviceContainer bottleneckDevices;
    bottleneckDevices.Add(centerLinkDevices.Get(0));
    bottleneckDevices.Add(centerLinkDevices.Get(1));

    queueDiscs = queueDiscHelper.Install(bottleneckDevices);

    // Install OnOff application on right-side nodes
    uint16_t port = 50000;
    for (int i = 0; i < nLeafNodes; ++i) {
        OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address(InetSocketAddress(rightRouterInterfaces.GetAddress(0), port)));
        onOffHelper.SetConstantRate(DataRate(dataRate), packetSize);

        ApplicationContainer app = onOffHelper.Install(leftLeafNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(10.0));
        port++;
    }

    // Sink application on right-side nodes
    for (int i = 0; i < nLeafNodes; ++i) {
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(rightRouterInterfaces.GetAddress(0), 50000 + i));
        ApplicationContainer sinkApp = sinkHelper.Install(rightLeafNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(11.0));
    }

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    if (enablePcap) {
        routerLink.EnablePcapAll("dumbbell");
    }

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        const FlowId flowId = it->first;
        const FlowMonitor::FlowStats &flowStats = it->second;
        const Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        std::cout << "Flow " << flowId << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flowStats.txPackets << "\n";
        std::cout << "  Rx Packets: " << flowStats.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flowStats.lostPackets << "\n";
        std::cout << "  Throughput: " << flowStats.rxBytes * 8.0 / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    // Queue Disc Statistics
    for (uint32_t i = 0; i < queueDiscs.GetN(); ++i) {
        Ptr<QueueDisc> queueDisc = queueDiscs.Get(i);
        if (queueDisc) {
            std::cout << "QueueDisc " << queueDisc->GetInstanceTypeId().GetName() << " statistics:\n";
            if (queueDisc->GetInstanceTypeId() == RedQueueDisc::GetTypeId()) {
                Ptr<RedQueueDisc> redQueue = DynamicCast<RedQueueDisc>(queueDisc);
                if (redQueue) {
                    std::cout << "  Queue length: " << redQueue->GetQueueLength().GetPackets() << " packets\n";
                    std::cout << "  Drop count: " << redQueue->GetDropCount() << " packets\n";
                    std::cout << "  Mark count: " << redQueue->GetMarkCount() << " packets\n";
                } else {
                    std::cerr << "Failed to cast QueueDisc to RedQueueDisc." << std::endl;
                }
            } else if (queueDisc->GetInstanceTypeId() == NlRedQueueDisc::GetTypeId()) {
                Ptr<NlRedQueueDisc> nlredQueue = DynamicCast<NlRedQueueDisc>(queueDisc);
                if (nlredQueue) {
                    std::cout << "  Queue length: " << nlredQueue->GetQueueLength().GetPackets() << " packets\n";
                    std::cout << "  Drop count: " << nlredQueue->GetDropCount() << " packets\n";
                    std::cout << "  Mark count: " << nlredQueue->GetMarkCount() << " packets\n";
                } else {
                    std::cerr << "Failed to cast QueueDisc to NlRedQueueDisc." << std::endl;
                }
            } else {
                std::cerr << "Unknown QueueDisc type." << std::endl;
            }
        } else {
            std::cerr << "QueueDisc pointer is null." << std::endl;
        }
    }

    monitor->SerializeToXmlFile("dumbbell.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}