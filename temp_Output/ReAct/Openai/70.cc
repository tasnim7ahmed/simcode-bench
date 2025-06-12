#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellRedNlredConfigurable");

int
main(int argc, char *argv[])
{
    uint32_t numLeaf = 2;
    std::string queueType = "RED";
    std::string mode = "Packets";
    std::string bottleneckDataRate = "10Mbps";
    std::string leafDataRate = "100Mbps";
    std::string bottleneckDelay = "5ms";
    std::string leafDelay = "2ms";
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 300;
    uint32_t maxBytes = 300 * 1024;
    double redMinTh = 5;
    double redMaxTh = 15;
    double redQw = 0.002;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numLeaf", "Number of left/right leaf nodes.", numLeaf);
    cmd.AddValue("queueType", "Queue discipline type: RED or NLRED", queueType);
    cmd.AddValue("mode", "Queue disc mode: Packets or Bytes", mode);
    cmd.AddValue("bottleneckDataRate", "Bottleneck data rate", bottleneckDataRate);
    cmd.AddValue("leafDataRate", "Leaf data rate", leafDataRate);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("leafDelay", "Leaf link delay", leafDelay);
    cmd.AddValue("packetSize", "App packet size (bytes)", packetSize);
    cmd.AddValue("maxPackets", "Queue disc limit in packets", maxPackets);
    cmd.AddValue("maxBytes", "Queue disc limit in bytes", maxBytes);
    cmd.AddValue("redMinTh", "RED/NLRED min threshold (packets or bytes)", redMinTh);
    cmd.AddValue("redMaxTh", "RED/NLRED max threshold (packets or bytes)", redMaxTh);
    cmd.AddValue("redQw", "RED/NLRED weight parameter", redQw);

    cmd.Parse(argc, argv);

    if (numLeaf < 1)
    {
        NS_LOG_ERROR("numLeaf must be >= 1");
        return 1;
    }
    if (!(queueType == "RED" || queueType == "NLRED"))
    {
        NS_LOG_ERROR("queueType must be either 'RED' or 'NLRED'");
        return 1;
    }
    if (!(mode == "Packets" || mode == "Bytes"))
    {
        NS_LOG_ERROR("mode must be either 'Packets' or 'Bytes'");
        return 1;
    }
    if (maxPackets == 0 && mode == "Packets")
    {
        NS_LOG_ERROR("maxPackets must be > 0 when mode=Packets");
        return 1;
    }
    if (maxBytes == 0 && mode == "Bytes")
    {
        NS_LOG_ERROR("maxBytes must be > 0 when mode=Bytes");
        return 1;
    }
    if (redMinTh < 0 || redMaxTh < redMinTh)
    {
        NS_LOG_ERROR("Invalid RED thresholds");
        return 1;
    }

    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    NodeContainer leftNodes, rightNodes;
    leftNodes.Create(numLeaf);
    rightNodes.Create(numLeaf);
    NodeContainer routers;
    routers.Create(2);

    PointToPointHelper leafHelper;
    leafHelper.SetDeviceAttribute("DataRate", StringValue(leafDataRate));
    leafHelper.SetChannelAttribute("Delay", StringValue(leafDelay));
    NetDeviceContainer leftDevices, rightDevices, routerLeftDevices, routerRightDevices;

    for (uint32_t i = 0; i < numLeaf; ++i)
    {
        NetDeviceContainer d = leafHelper.Install(leftNodes.Get(i), routers.Get(0));
        leftDevices.Add(d.Get(0));
        routerLeftDevices.Add(d.Get(1));

        d = leafHelper.Install(rightNodes.Get(i), routers.Get(1));
        rightDevices.Add(d.Get(0));
        routerRightDevices.Add(d.Get(1));
    }

    PointToPointHelper coreHelper;
    coreHelper.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    coreHelper.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    NetDeviceContainer routerDevices = coreHelper.Install(routers.Get(0), routers.Get(1));

    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftIntfs, rightIntfs;
    for (uint32_t i = 0; i < numLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        leftIntfs.push_back(address.Assign(NetDeviceContainer(leftDevices.Get(i), routerLeftDevices.Get(i))));
    }
    for (uint32_t i = 0; i < numLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        rightIntfs.push_back(address.Assign(NetDeviceContainer(rightDevices.Get(i), routerRightDevices.Get(i))));
    }
    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIfs = address.Assign(routerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    TrafficControlHelper tch;
    std::string queueDiscType, queueDiscMode;
    if (queueType == "RED")
        queueDiscType = "ns3::RedQueueDisc";
    else
        queueDiscType = "ns3::NlredQueueDisc";

    if (mode == "Packets")
        queueDiscMode = "QUEUE_MODE_PACKETS";
    else
        queueDiscMode = "QUEUE_MODE_BYTES";

    Config::SetDefault(queueDiscType + "::Mode", StringValue(queueDiscMode));
    if (mode == "Packets")
    {
        Config::SetDefault(queueDiscType + "::MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, maxPackets)));
        Config::SetDefault(queueDiscType + "::MinTh", DoubleValue(redMinTh));
        Config::SetDefault(queueDiscType + "::MaxTh", DoubleValue(redMaxTh));
    }
    else
    {
        Config::SetDefault(queueDiscType + "::MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, maxBytes)));
        Config::SetDefault(queueDiscType + "::MinTh", DoubleValue(redMinTh));
        Config::SetDefault(queueDiscType + "::MaxTh", DoubleValue(redMaxTh));
    }
    Config::SetDefault(queueDiscType + "::QW", DoubleValue(redQw));

    tch.SetRootQueueDisc(queueDiscType);

    Ptr<QueueDisc> queueDisc = tch.Install(routerDevices.Get(0)).Get(0);

    ApplicationContainer sinkApps, onoffApps;
    uint16_t basePort = 9000;

    for (uint32_t i = 0; i < numLeaf; ++i)
    {
        Address sinkAddress(InetSocketAddress(leftIntfs[i].GetAddress(0), basePort + i));
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), basePort + i));
        sinkApps.Add(sinkHelper.Install(leftNodes.Get(i)));

        Ptr<ExponentialRandomVariable> onRV = CreateObject<ExponentialRandomVariable>();
        Ptr<ExponentialRandomVariable> offRV = CreateObject<ExponentialRandomVariable>();
        onRV->SetAttribute("Mean", DoubleValue(1.0));
        offRV->SetAttribute("Mean", DoubleValue(0.5));

        OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("8Mbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=1.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));
        onoffApps.Add(onoff.Install(rightNodes.Get(i)));
    }

    sinkApps.Start(Seconds(0.1));
    onoffApps.Start(Seconds(0.2));
    sinkApps.Stop(Seconds(12.0));
    onoffApps.Stop(Seconds(12.0));

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    if (!queueDisc)
    {
        NS_LOG_ERROR("QueueDisc not found or not installed properly.");
    }
    else
    {
        uint32_t nDrops = 0;
        for (const auto &stat : queueDisc->GetStats())
        {
            if (stat.first.find("nDrops") != std::string::npos)
                nDrops += stat.second;
        }
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Queue Type: " << queueType << ", Mode: " << mode << std::endl;
        std::cout << "Queue Disc Limits - ";
        if (mode == "Packets")
            std::cout << "MaxPackets: " << maxPackets;
        else
            std::cout << "MaxBytes: " << maxBytes;
        std::cout << std::endl;
        std::cout << "RED/NLRED thresholds: minTh=" << redMinTh << ", maxTh=" << redMaxTh << std::endl;
        std::cout << "Total packet drops at bottleneck queue: " << nDrops << std::endl;

        for (const auto &kv : queueDisc->GetStats())
        {
            std::cout << kv.first << ": " << kv.second << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}