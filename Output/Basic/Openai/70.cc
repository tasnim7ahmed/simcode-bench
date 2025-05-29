#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellRedNlredExample");

int main(int argc, char *argv[])
{
    // Default parameters
    uint32_t nLeaf = 4;
    std::string queueType = "RED"; // RED or NLRED
    uint32_t maxPackets = 1000;
    uint32_t maxBytes = 0;
    uint32_t pktSize = 1024;
    std::string bottleneckDataRate = "10Mbps";
    std::string leafDataRate = "100Mbps";
    std::string bottleneckDelay = "2ms";
    std::string leafDelay = "1ms";
    double minTh = 5;
    double maxTh = 15;
    bool isByteMode = false;
    double simTime = 10.0;
    double meanOn = 0.5;
    double meanOff = 0.5;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeaf", "Number of left and right leaf nodes", nLeaf);
    cmd.AddValue("queueType", "QueueDisc type: RED or NLRED", queueType);
    cmd.AddValue("maxPackets", "Queue disc limit (packets)", maxPackets);
    cmd.AddValue("maxBytes", "Queue disc limit (bytes, overrides packets if > 0)", maxBytes);
    cmd.AddValue("pktSize", "Application packet size (bytes)", pktSize);
    cmd.AddValue("bottleneckDataRate", "Bottleneck point-to-point link data rate", bottleneckDataRate);
    cmd.AddValue("leafDataRate", "Access point-to-point link data rate", leafDataRate);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("leafDelay", "Access link delay", leafDelay);
    cmd.AddValue("minTh", "RED min threshold (in packets or bytes)", minTh);
    cmd.AddValue("maxTh", "RED max threshold (in packets or bytes)", maxTh);
    cmd.AddValue("isByteMode", "Queue mode: true=byte-based, false=packet-based", isByteMode);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.AddValue("meanOn", "Mean ON time value for OnOff app (s)", meanOn);
    cmd.AddValue("meanOff", "Mean OFF time value for OnOff app (s)", meanOff);
    cmd.Parse(argc, argv);

    if (nLeaf < 1)
    {
        NS_LOG_ERROR("Number of leaf nodes must be >= 1");
        return 1;
    }
    if (queueType != "RED" && queueType != "NLRED")
    {
        NS_LOG_ERROR("Invalid queueType (must be RED or NLRED)");
        return 1;
    }
    if (maxPackets == 0 && maxBytes == 0)
    {
        NS_LOG_ERROR("Either maxPackets or maxBytes must be set > 0");
        return 1;
    }

    std::string queueDiscType = (queueType == "RED" ? "ns3::RedQueueDisc" : "ns3::NlRedQueueDisc");

    // Create dumbbell topology
    PointToPointHelper leafHelper;
    leafHelper.SetDeviceAttribute("DataRate", StringValue(leafDataRate));
    leafHelper.SetChannelAttribute("Delay", StringValue(leafDelay));

    PointToPointHelper bottleneckHelper;
    bottleneckHelper.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    bottleneckHelper.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NodeContainer leftNodes;
    NodeContainer rightNodes;
    leftNodes.Create(nLeaf);
    rightNodes.Create(nLeaf);
    NodeContainer routers;
    routers.Create(2);

    // Left and right links
    NetDeviceContainer leftDevices;
    NetDeviceContainer rightDevices;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer leftPair = NodeContainer(leftNodes.Get(i), routers.Get(0));
        NetDeviceContainer ld = leafHelper.Install(leftPair);
        leftDevices.Add(ld.Get(0));
    }
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer rightPair = NodeContainer(routers.Get(1), rightNodes.Get(i));
        NetDeviceContainer rd = leafHelper.Install(rightPair);
        rightDevices.Add(rd.Get(1));
    }

    // Bottleneck link
    NetDeviceContainer routerDevices = bottleneckHelper.Install(routers);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftInterfaces;
    std::vector<Ipv4InterfaceContainer> rightInterfaces;
    // Left side
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        NetDeviceContainer nd;
        nd.Add(leftDevices.Get(i));
        nd.Add(routers.Get(0)->GetDevice(i + 1));
        leftInterfaces.push_back(address.Assign(nd));
        address.NewNetwork();
    }
    // Right side
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        NetDeviceContainer nd;
        nd.Add(routers.Get(1)->GetDevice(i + 1));
        nd.Add(rightDevices.Get(i));
        rightInterfaces.push_back(address.Assign(nd));
        address.NewNetwork();
    }
    // Router-router link
    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIf = address.Assign(routerDevices);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic Control (RED or NLRED) on router-router bottleneck
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc(queueDiscType);

    if (!isByteMode)
    {
        tch.SetQueueLimits("ns3::QueueLimits",
                           "MaxPackets", UintegerValue(maxPackets),
                           "MaxBytes", UintegerValue(0));
    }
    else
    {
        tch.SetQueueLimits("ns3::QueueLimits",
                           "MaxPackets", UintegerValue(0),
                           "MaxBytes", UintegerValue(maxBytes));
    }

    Config::SetDefault(queueDiscType + "::Mode",
                       EnumValue(isByteMode ? QueueDiscMode::QUEUE_MODE_BYTES : QueueDiscMode::QUEUE_MODE_PACKETS));
    if (!isByteMode)
    {
        Config::SetDefault(queueDiscType + "::MinTh", DoubleValue(minTh));
        Config::SetDefault(queueDiscType + "::MaxTh", DoubleValue(maxTh));
    }
    else
    {
        Config::SetDefault(queueDiscType + "::MinTh", DoubleValue(minTh * pktSize));
        Config::SetDefault(queueDiscType + "::MaxTh", DoubleValue(maxTh * pktSize));
    }

    QueueDiscContainer qdiscs = tch.Install(routerDevices);

    // OnOff Applications: each right node is sink, left node is source
    ApplicationContainer sinkApps;
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        Address sinkAddress(InetSocketAddress(rightInterfaces[i].GetAddress(1), 9000 + i));
        PacketSinkHelper sink("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sink.Install(rightNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime + 1));
        sinkApps.Add(sinkApp);

        OnOffHelper client("ns3::TcpSocketFactory", sinkAddress);
        client.SetAttribute("PacketSize", UintegerValue(pktSize));
        client.SetAttribute("DataRate", StringValue(leafDataRate));
        client.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=" + std::to_string(meanOn) + "]"));
        client.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=" + std::to_string(meanOff) + "]"));
        ApplicationContainer clientApp = client.Install(leftNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
        clientApps.Add(clientApp);
    }

    Simulator::Stop(Seconds(simTime + 2));

    Simulator::Run();

    // Queue disc statistics
    Ptr<QueueDisc> q = qdiscs.Get(0);
    QueueDisc::Stats stats = q->GetStats();
    std::cout << "QueueDisc: " << queueType << std::endl;
    std::cout << "Packets dropped by DropTail: "
              << stats.nDrop << std::endl;
    std::cout << "Packets dropped by RED/NLRED: "
              << stats.nMark << std::endl;
    std::cout << "Packets marked (ECN): "
              << stats.nMarkecn << std::endl;

    Simulator::Destroy();
    return 0;
}