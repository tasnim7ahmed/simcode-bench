#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <string>

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Default parameters
    uint32_t nLeaf = 4;
    std::string queueType = "RED";
    uint32_t maxPackets = 1000;
    uint32_t packetSize = 1000; // bytes
    std::string dataRate = "10Mbps";
    std::string bottleneckDataRate = "2Mbps";
    std::string accessDelay = "2ms";
    std::string bottleneckDelay = "20ms";
    double simTime = 10.0; // seconds

    // RED parameters
    double minTh = 5;
    double maxTh = 15;
    double qW = 0.002;
    double lInterm = 0.02;

    // Command line parsing
    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of left and right leaf nodes", nLeaf);
    cmd.AddValue("queueType", "Queue type: RED or FRED", queueType);
    cmd.AddValue("maxPackets", "Max packets in queue", maxPackets);
    cmd.AddValue("packetSize", "Application packet size in bytes", packetSize);
    cmd.AddValue("dataRate", "Access link data rate", dataRate);
    cmd.AddValue("bottleneckDataRate", "Bottleneck link data rate", bottleneckDataRate);
    cmd.AddValue("accessDelay", "Access link delay", accessDelay);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("minTh", "RED minimum threshold (packets)", minTh);
    cmd.AddValue("maxTh", "RED maximum threshold (packets)", maxTh);
    cmd.AddValue("qW", "RED queue weight", qW);
    cmd.AddValue("lInterm", "RED lInterm", lInterm);
    cmd.Parse(argc, argv);

    if (queueType != "RED" && queueType != "FRED")
    {
        NS_ABORT_MSG("queueType must be RED or FRED");
    }

    // Create dumbbell topology
    NodeContainer leftLeaf, rightLeaf, routers;
    leftLeaf.Create(nLeaf);
    rightLeaf.Create(nLeaf);
    routers.Create(2);

    // PointToPoint links
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue(dataRate));
    accessLink.SetChannelAttribute("Delay", StringValue(accessDelay));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    // Connect leftLeaf to left router
    NetDeviceContainer leftDevices, leftRouterDevices;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer link = accessLink.Install(leftLeaf.Get(i), routers.Get(0));
        leftDevices.Add(link.Get(0));
        leftRouterDevices.Add(link.Get(1));
    }

    // Connect rightLeaf to right router
    NetDeviceContainer rightDevices, rightRouterDevices;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer link = accessLink.Install(rightLeaf.Get(i), routers.Get(1));
        rightDevices.Add(link.Get(0));
        rightRouterDevices.Add(link.Get(1));
    }

    // Connect routers (bottleneck)
    NetDeviceContainer routerDevices = bottleneckLink.Install(routers);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(leftLeaf);
    stack.Install(rightLeaf);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftInterfaces, rightInterfaces;

    // Assign to left
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        leftInterfaces.push_back(address.Assign(NetDeviceContainer(leftDevices.Get(i), leftRouterDevices.Get(i))));
    }

    // Assign to right
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        rightInterfaces.push_back(address.Assign(NetDeviceContainer(rightDevices.Get(i), rightRouterDevices.Get(i))));
    }

    // Assign to bottleneck link
    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIf = address.Assign(routerDevices);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // QueueDisc on router[0] -> router[1] bottleneck outgoing device
    TrafficControlHelper tch;
    if (queueType == "RED")
    {
        tch.SetRootQueueDisc("ns3::RedQueueDisc",
                             "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, maxPackets)),
                             "MinTh", DoubleValue(minTh),
                             "MaxTh", DoubleValue(maxTh),
                             "LinkBandwidth", StringValue(bottleneckDataRate),
                             "LinkDelay", StringValue(bottleneckDelay),
                             "QW", DoubleValue(qW),
                             "LInterm", DoubleValue(lInterm));
    }
    else // FRED (Feng's Adaptive RED)
    {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }

    // router[0] outgoing device to router[1] (routerDevices.Get(0))
    QueueDiscContainer qdiscs = tch.Install(routerDevices.Get(0));

    // TCP OnOff app on rightLeaf, sink on leftLeaf
    uint16_t port = 10000;
    ApplicationContainer sinks, onOffApps;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        // Install PacketSink on leftLeaf
        Address sinkAddress(InetSocketAddress(leftInterfaces[i].GetAddress(0), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        sinks.Add(sinkHelper.Install(leftLeaf.Get(i)));

        // Install OnOff application on rightLeaf
        Ptr<UniformRandomVariable> onTime = CreateObject<UniformRandomVariable>();
        onTime->SetAttribute("Min", DoubleValue(0.5));
        onTime->SetAttribute("Max", DoubleValue(1.5));
        Ptr<UniformRandomVariable> offTime = CreateObject<UniformRandomVariable>();
        offTime->SetAttribute("Min", DoubleValue(0.1));
        offTime->SetAttribute("Max", DoubleValue(0.5));

        OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(bottleneckDataRate)));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("OnTime", PointerValue(onTime));
        onoff.SetAttribute("OffTime", PointerValue(offTime));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        onOffApps.Add(onoff.Install(rightLeaf.Get(i)));
    }

    sinks.Start(Seconds(0.0));
    sinks.Stop(Seconds(simTime + 1.0));

    // Enable FlowMonitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    // Display queue disc statistics
    Ptr<QueueDisc> q = qdiscs.Get(0);
    std::cout << "QueueDisc stats:" << std::endl;
    std::cout << "  Packets enqueued: " << q->GetStats().nTotalReceivedPackets << std::endl;
    std::cout << "  Packets dequeued: " << q->GetStats().nTotalDequeuedPackets << std::endl;
    std::cout << "  Packets dropped:  " << q->GetStats().nTotalDroppedPackets << std::endl;

    if (q->GetStats().nTotalDroppedPackets == 0)
    {
        std::cout << "*** Warning: No queue drops observed. Increase offered load or lower queue size to see drops." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}