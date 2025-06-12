#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/tcp-socket-base.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpReno"));

    NodeContainer nodes;
    nodes.Create(3);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("2ms"));
    bottleneckLink.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    PointToPointHelper leftLink;
    leftLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    leftLink.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper rightLink;
    rightLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    rightLink.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer leftDevices = leftLink.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer rightDevices = rightLink.Install(nodes.Get(2), nodes.Get(1));
    NetDeviceContainer bottleneckDevices = bottleneckLink.Install(nodes.Get(1), nodes.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces = address.Assign(leftDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces = address.Assign(rightDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(leftInterfaces.GetAddress(0), port));
    source1.SetAttribute("SendSize", UintegerValue(1024));
    source1.SetAttribute("MaxBytes", UintegerValue(10000000));
    ApplicationContainer sourceApps1 = source1.Install(nodes.Get(2));
    sourceApps1.Start(Seconds(1.0));
    sourceApps1.Stop(Seconds(10.0));

    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(0), port+1));
    source2.SetAttribute("SendSize", UintegerValue(1024));
    source2.SetAttribute("MaxBytes", UintegerValue(10000000));
    ApplicationContainer sourceApps2 = source2.Install(nodes.Get(0));
    sourceApps2.Start(Seconds(2.0));
    sourceApps2.Stop(Seconds(10.0));

    PacketSinkHelper sink1("ns3::TcpSocketFactory", InetSocketAddress(leftInterfaces.GetAddress(0), port));
    ApplicationContainer sinkApps1 = sink1.Install(nodes.Get(0));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(10.0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(0), port+1));
    ApplicationContainer sinkApps2 = sink2.Install(nodes.Get(2));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim("congestion.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 1.0);
    anim.SetConstantPosition(nodes.Get(1), 2.0, 1.0);
    anim.SetConstantPosition(nodes.Get(2), 4.0, 1.0);

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();

    return 0;
}