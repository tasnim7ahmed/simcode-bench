#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpTopology");

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation timing parameters
    double startTime = 0.0;
    double setupTime = 1.0;  // Startup window to stagger flows
    double convergenceTime = 3.0;
    double measurementDuration = 1.0;
    double stopTime = startTime + setupTime + convergenceTime + measurementDuration;

    // Bottleneck link configuration
    std::string link1DataRate = "10Gbps";
    std::string link2DataRate = "1Gbps";
    std::string delay = "1ms";

    // Create nodes
    NodeContainer hosts;
    hosts.Create(50); // H0-H49: senders

    NodeContainer switches;
    switches.Create(2); // T1 and T2

    NodeContainer router;
    router.Create(1); // R1

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");

    // Connect hosts to T1 (switches.Get(0))
    PointToPointHelper hostLink;
    hostLink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    hostLink.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer hostSwitchDevices[50];
    Ipv4InterfaceContainer hostInterfaces[50];

    for (uint32_t i = 0; i < 50; ++i) {
        hostSwitchDevices[i] = hostLink.Install(hosts.Get(i), switches.Get(0));
        Ipv4InterfaceContainer interfaces = ip.Assign(hostSwitchDevices[i]);
        hostInterfaces[i] = interfaces;
        ip.NewNetwork();
    }

    // Link between T1 and T2 (bottleneck 1: 10 Gbps with RED/ECN)
    PointToPointHelper bottleneck1;
    bottleneck1.SetDeviceAttribute("DataRate", StringValue(link1DataRate));
    bottleneck1.SetChannelAttribute("Delay", StringValue(delay));

    // RED queue configuration for ECN marking on T1-T2 link
    TrafficControlHelper tchBottleneck1;
    tchBottleneck1.SetRootQueueDisc("ns3::RedQueueDisc",
                                   "MaxPackets", UintegerValue(1000),
                                   "MinTh", DoubleValue(5),
                                   "MaxTh", DoubleValue(15),
                                   "QW", DoubleValue(0.002),
                                   "LInterm", DoubleValue(10),
                                   "ECN", BooleanValue(true),
                                   "Wait", BooleanValue(true),
                                   "Gentle", BooleanValue(false));
    bottleneck1.SetQueue("ns3::DropTailQueue<Packet>");

    NetDeviceContainer t1t2Devices = bottleneck1.Install(switches.Get(0), switches.Get(1));
    Ipv4InterfaceContainer t1t2Interfaces = ip.Assign(t1t2Devices);
    ip.NewNetwork();

    // Link between T2 and R1 (bottleneck 2: 1 Gbps with RED/ECN)
    PointToPointHelper bottleneck2;
    bottleneck2.SetDeviceAttribute("DataRate", StringValue(link2DataRate));
    bottleneck2.SetChannelAttribute("Delay", StringValue(delay));

    // RED queue for ECN marking on T2-R1 link
    TrafficControlHelper tchBottleneck2;
    tchBottleneck2.SetRootQueueDisc("ns3::RedQueueDisc",
                                   "MaxPackets", UintegerValue(1000),
                                   "MinTh", DoubleValue(5),
                                   "MaxTh", DoubleValue(15),
                                   "QW", DoubleValue(0.002),
                                   "LInterm", DoubleValue(10),
                                   "ECN", BooleanValue(true),
                                   "Wait", BooleanValue(true),
                                   "Gentle", BooleanValue(false));
    bottleneck2.SetQueue("ns3::DropTailQueue<Packet>");

    NetDeviceContainer t2r1Devices = bottleneck2.Install(switches.Get(1), router.Get(0));
    Ipv4InterfaceContainer t2r1Interfaces = ip.Assign(t2r1Devices);
    ip.NewNetwork();

    // Router R1 to destination network - create dummy destinations
    NodeContainer dests;
    dests.Create(10); // D0-D9
    stack.Install(dests);

    PointToPointHelper destLink;
    destLink.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    destLink.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer r1DestDevices[10];
    Ipv4InterfaceContainer destInterfaces[10];

    for (uint32_t i = 0; i < 10; ++i) {
        r1DestDevices[i] = destLink.Install(router.Get(0), dests.Get(i));
        Ipv4InterfaceContainer interfaces = ip.Assign(r1DestDevices[i]);
        destInterfaces[i] = interfaces;
        ip.NewNetwork();
    }

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP flows staggered over 1 second
    uint16_t sinkPort = 50000;
    ApplicationContainer sinkApps;
    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    for (uint32_t i = 0; i < 10; ++i) {
        ApplicationContainer app = packetSink.Install(dests.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(stopTime));
    }

    ApplicationContainer clientApps;
    double flowInterval = setupTime / 40.0; // Stagger interval

    for (uint32_t i = 0; i < 40; ++i) {
        uint32_t destIndex = i % 10;
        Address sinkAddress(InetSocketAddress(destInterfaces[destIndex].GetAddress(0), sinkPort));

        OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
        client.SetAttribute("PacketSize", UintegerValue(1460));
        client.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite data

        ApplicationContainer app = client.Install(hosts.Get(i));
        double start = startTime + i * flowInterval;
        app.Start(Seconds(start));
        app.Stop(Seconds(stopTime));
    }

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Output stats after measurement period
    Simulator::Stop(Seconds(stopTime));

    Simulator::Run();

    // Output throughput per flow, aggregate fairness, queue stats
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    uint32_t flowCount = 0;

    std::cout << "Throughput per flow:" << std::endl;
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (InetSocketAddress::IsMatchingType(t.destinationAddress, sinkPort)) {
            double duration = measurementDuration;
            double txBytes = iter->second.txBytes;
            double throughput = (txBytes * 8.0) / duration / 1e6; // Mbps
            std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "): "
                      << throughput << " Mbps" << std::endl;
            totalThroughput += throughput;
            flowCount++;
        }
    }

    std::cout << "\nAggregate throughput: " << totalThroughput << " Mbps" << std::endl;
    if (flowCount > 0) {
        std::cout << "Average throughput per flow: " << (totalThroughput / flowCount) << " Mbps" << std::endl;
    }

    std::cout << "\nQueue statistics at bottlenecks:" << std::endl;
    for (size_t i = 0; i < t1t2Devices.GetN(); ++i) {
        Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(t1t2Devices.Get(i));
        if (dev) {
            Ptr<Queue<Packet>> queue = dev->GetQueue();
            std::cout << "T1-T2 device " << i << " packets in queue: " << queue->GetNPackets() << std::endl;
        }
    }

    for (size_t i = 0; i < t2r1Devices.GetN(); ++i) {
        Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(t2r1Devices.Get(i));
        if (dev) {
            Ptr<Queue<Packet>> queue = dev->GetQueue();
            std::cout << "T2-R1 device " << i << " packets in queue: " << queue->GetNPackets() << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}