/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Dumbbell topology with configurable queue types (RED/NLRED),
 * leaf node count, queue settings, data rates, and RED thresholds.
 * Statistics of queue drops are displayed at the end.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ConfigurableDumbbellRedNlred");

int
main(int argc, char *argv[])
{
    // Simulation parameters with default values
    uint32_t nLeaf = 4;
    std::string queueType = "RED";
    std::string mode = "Packets"; // or "Bytes"
    uint32_t maxSize = 1000; // in packets (or bytes if mode="Bytes")
    uint32_t pktSize = 1024; // bytes
    std::string bottleneckDataRate = "10Mbps";
    std::string bottleneckDelay = "2ms";
    std::string leafDataRate = "100Mbps";
    std::string leafDelay = "1ms";
    double minTh = 5.0; // in packets or bytes
    double maxTh = 15.0; // in packets or bytes
    double qW = 0.002;
    double maxP = 1.0;
    std::string queueDiscType = "ns3::RedQueueDisc";
    uint32_t seed = 1; // for randomization

    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeaf", "Number of left/right leaf nodes", nLeaf);
    cmd.AddValue("queueType", "Queue type: RED or NLRED", queueType);
    cmd.AddValue("mode", "Queue operation mode: Packets or Bytes", mode);
    cmd.AddValue("maxSize", "RED max queue size (packets or bytes)", maxSize);
    cmd.AddValue("pktSize", "Packet size (bytes)", pktSize);
    cmd.AddValue("bottleneckDataRate", "Bottleneck link data rate", bottleneckDataRate);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("leafDataRate", "Leaf link data rate", leafDataRate);
    cmd.AddValue("leafDelay", "Leaf link delay", leafDelay);
    cmd.AddValue("minTh", "RED minimum threshold (packets/bytes)", minTh);
    cmd.AddValue("maxTh", "RED maximum threshold (packets/bytes)", maxTh);
    cmd.AddValue("qW", "RED queue weight (w_q)", qW);
    cmd.AddValue("maxP", "RED max_p (drop probability)", maxP);
    cmd.AddValue("seed", "Random seed for OnOff apps", seed);
    cmd.Parse(argc, argv);

    if (queueType == "RED" || queueType == "red")
    {
        queueDiscType = "ns3::RedQueueDisc";
    }
    else if (queueType == "NLRED" || queueType == "nlred")
    {
        queueDiscType = "ns3::NlredQueueDisc";
    }
    else
    {
        NS_ABORT_MSG("Unknown queue type (should be RED or NLRED)");
    }

    if (nLeaf < 1)
    {
        NS_ABORT_MSG("Number of leaf nodes must be at least 1");
    }

    // Create dumbbell topology
    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create(nLeaf);
    rightNodes.Create(nLeaf);
    routers.Create(2);

    // Point-to-point helpers
    PointToPointHelper leafHelper;
    leafHelper.SetDeviceAttribute("DataRate", StringValue(leafDataRate));
    leafHelper.SetChannelAttribute("Delay", StringValue(leafDelay));
    leafHelper.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p")); // Leaf links: DropTail

    PointToPointHelper routerHelper;
    routerHelper.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    routerHelper.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    routerHelper.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

    NetDeviceContainer leftDevices, rightDevices, routerDevices;

    // Connect left nodes to left router
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer link = leafHelper.Install(leftNodes.Get(i), routers.Get(0));
        leftDevices.Add(link.Get(0));
    }
    // Connect right nodes to right router
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer link = leafHelper.Install(rightNodes.Get(i), routers.Get(1));
        rightDevices.Add(link.Get(0));
    }
    // Connect routers (bottleneck)
    routerDevices = routerHelper.Install(routers);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftIfs, rightIfs;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        leftIfs.push_back(address.Assign(NetDeviceContainer(leftDevices.Get(i), routers.Get(0)->GetDevice(i))));
    }
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        rightIfs.push_back(address.Assign(NetDeviceContainer(rightDevices.Get(i), routers.Get(1)->GetDevice(i))));
    }

    // Bottleneck link ip assignment
    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIfs = address.Assign(routerDevices);

    // Install RED or NLRED on the bottleneck link
    TrafficControlHelper tch;
    std::ostringstream maxSizeStr;
    if (mode == "Packets" || mode == "packets")
        maxSizeStr << maxSize << "p";
    else
        maxSizeStr << maxSize << "b";

    Config::SetDefault(queueDiscType + "::Mode",
                       StringValue((mode == "Bytes" || mode == "bytes") ? "QUEUE_MODE_BYTES" : "QUEUE_MODE_PACKETS"));

    // Set minTh/maxTh as packets or bytes
    Config::SetDefault(queueDiscType + "::MinTh", DoubleValue(minTh));
    Config::SetDefault(queueDiscType + "::MaxTh", DoubleValue(maxTh));
    Config::SetDefault(queueDiscType + "::LinkBandwidth", StringValue(bottleneckDataRate));
    Config::SetDefault(queueDiscType + "::LinkDelay", StringValue(bottleneckDelay));
    Config::SetDefault(queueDiscType + "::QW", DoubleValue(qW));
    Config::SetDefault(queueDiscType + "::LInterm", DoubleValue(50)); // default
    Config::SetDefault(queueDiscType + "::MaxP", DoubleValue(maxP));
    tch.SetRootQueueDisc(queueDiscType, "MaxSize", QueueSizeValue(QueueSize(maxSizeStr.str())));

    // Install on both directions of bottleneck link (typically both devices)
    QueueDiscContainer qdiscs = tch.Install(routerDevices);

    // Install OnOffApplication on right-side nodes, towards left-side nodes (all-to-all)
    uint16_t port = 50000;
    ApplicationContainer appCont;

    // Use different On/Off times for each app (random, with the seed specified)
    RngSeedManager::SetSeed(seed);
    Ptr<UniformRandomVariable> onVar = CreateObject<UniformRandomVariable>();
    Ptr<UniformRandomVariable> offVar = CreateObject<UniformRandomVariable>();
    onVar->SetAttribute("Min", DoubleValue(1.0));
    onVar->SetAttribute("Max", DoubleValue(3.0));
    offVar->SetAttribute("Min", DoubleValue(1.0));
    offVar->SetAttribute("Max", DoubleValue(4.0));
    
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        Address sinkAddress(InetSocketAddress(leftIfs[i].GetAddress(0), port + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + i));
        ApplicationContainer sink = sinkHelper.Install(leftNodes.Get(i));
        sink.Start(Seconds(0.0));
        sink.Stop(Seconds(20.0));

        OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
        onoff.SetAttribute("DataRate", StringValue("20Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(pktSize));
        onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=4.0]"));
        ApplicationContainer app = onoff.Install(rightNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(20.0));
        appCont.Add(app);
    }

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Simulation
    Simulator::Stop(Seconds(21.0));
    Simulator::Run();

    // Collect statistics about packet drops in the queue
    Ptr<QueueDisc> q = qdiscs.Get(0); // Only look at one direction (left-to-right)
    uint32_t nTotalDropped = 0;
    uint32_t nMark = 0;
    if (q)
    {
        uint32_t nDrop = q->GetStats().GetTotalDroppedPackets();
        uint32_t nMarkTmp = q->GetStats().GetTotalMarkedPackets();
        std::cout << "\n[" << queueType << "] queue disc statistics: ";
        std::cout << "\n  Total packets dropped: " << nDrop;
        std::cout << "\n  Total packets marked (ECN): " << nMarkTmp << "\n";
        nTotalDropped = nDrop;
        nMark = nMarkTmp;
    }
    else
    {
        std::cout << "Error: Could not retrieve QueueDisc statistics!\n";
    }

    Simulator::Destroy();
    return 0;
}