#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrafficControlSimulation");

int main(int argc, char *argv[]) {
    bool enablePcapTracing = false;
    bool enableDCTCP = false;
    std::string bottleneckQueueDisc = "fqcodel";

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcapTracing);
    cmd.AddValue("enableDCTCP", "Enable DCTCP", enableDCTCP);
    cmd.AddValue("bottleneckQueueDisc", "Bottleneck queue disc type (fqcodel)", bottleneckQueueDisc);

    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetLevel(TrafficControlSimulation::GetTypeId(), LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic Control Configuration
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc", "MaxSize", StringValue("1000p"));

    QueueDiscContainer queueDiscs;
    queueDiscs.Add(tch.Install(devices2.Get(0)));

    // Applications
    uint16_t port = 9;  // Discard port (RFC 863)

    // Client 1
    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(interfaces3.GetAddress(0), port));
    source1.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps1 = source1.Install(nodes.Get(0));
    sourceApps1.Start(Seconds(1.0));
    sourceApps1.Stop(Seconds(10.0));

    PacketSinkHelper sink1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps1 = sink1.Install(nodes.Get(3));
    sinkApps1.Start(Seconds(0.0));
    sinkApps1.Stop(Seconds(10.0));


    // Client 2
    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(interfaces3.GetAddress(0), port+1));
    source2.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps2 = source2.Install(nodes.Get(0));
    sourceApps2.Start(Seconds(1.0));
    sourceApps2.Stop(Seconds(10.0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port+1));
    ApplicationContainer sinkApps2 = sink2.Install(nodes.Get(3));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(10.0));

    // Client 3
    BulkSendHelper source3("ns3::TcpSocketFactory", InetSocketAddress(interfaces3.GetAddress(0), port+2));
    source3.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApps3 = source3.Install(nodes.Get(0));
    sourceApps3.Start(Seconds(1.0));
    sourceApps3.Stop(Seconds(10.0));

    PacketSinkHelper sink3("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port+2));
    ApplicationContainer sinkApps3 = sink3.Install(nodes.Get(3));
    sinkApps3.Start(Seconds(0.0));
    sinkApps3.Stop(Seconds(10.0));

    // Ping Application
    PingHelper ping(interfaces3.GetAddress(0));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(2.0));
    pingApp.Stop(Seconds(10.0));

    // Tracing
    if (enablePcapTracing) {
        pointToPoint.EnablePcapAll("traffic-control");
    }

    // Queue Disc Tracing
    // No direct drop trace for FqCoDel, using Qdisc::Drop
    Config::ConnectWithoutContext("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Qdisc/Drop", MakeCallback(&QueueDisc::TraceDrop));
    Config::ConnectWithoutContext("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Qdisc/Mark", MakeCallback(&QueueDisc::TraceMark));
    Config::ConnectWithoutContext("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Qdisc/QueueSize", MakeCallback(&QueueDisc::TraceQueueSize));

    // Monitor Routing Protocols
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

    monitor->SerializeToXmlFile("traffic-control.flowmon", true, true);
    Simulator::Destroy();
    return 0;
}