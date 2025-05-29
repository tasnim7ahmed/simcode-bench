#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t nLeaf = 4;
    std::string queueType = "RED";
    bool byteMode = false;
    uint32_t packSize = 1024;
    std::string bottleneckDatarate = "10Mbps";
    std::string bottleneckDelay = "10ms";
    std::string leafDatarate = "100Mbps";
    std::string leafDelay = "2ms";
    uint32_t redQDiscLimit = 100; // For packets if in packetMode, bytes otherwise.
    uint32_t minTh = 30;
    uint32_t maxTh = 90;
    double redQW = 0.002;

    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of left and right leaf nodes", nLeaf);
    cmd.AddValue("queueType", "Queue type: RED or NLRED", queueType);
    cmd.AddValue("byteMode", "Enable RED/NLRED byte mode", byteMode);
    cmd.AddValue("packSize", "Application packet size (bytes)", packSize);
    cmd.AddValue("bottleneckDatarate", "Datarate for dumbbell bottleneck link", bottleneckDatarate);
    cmd.AddValue("bottleneckDelay", "Delay for bottleneck link", bottleneckDelay);
    cmd.AddValue("leafDatarate", "Datarate for leaf links", leafDatarate);
    cmd.AddValue("leafDelay", "Delay for leaf links", leafDelay);
    cmd.AddValue("redQDiscLimit", "RED queue disc limit (packets or bytes)", redQDiscLimit);
    cmd.AddValue("minTh", "RED min threshold (packets or bytes)", minTh);
    cmd.AddValue("maxTh", "RED max threshold (packets or bytes)", maxTh);
    cmd.AddValue("redQW", "RED queue weight parameter", redQW);
    cmd.Parse(argc, argv);

    if(queueType != "RED" && queueType != "NLRED")
    {
        NS_ABORT_MSG_UNLESS(false, "Invalid queueType. Choose either RED or NLRED.");
    }

    if(nLeaf < 1)
    {
        NS_ABORT_MSG_UNLESS(false, "At least one leaf node required.");
    }

    // Create dumbbell topology
    PointToPointHelper leafLink;
    leafLink.SetDeviceAttribute("DataRate", StringValue(leafDatarate));
    leafLink.SetChannelAttribute("Delay", StringValue(leafDelay));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckDatarate));
    bottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    // nLeaf per side
    NodeContainer leftNodes;
    leftNodes.Create(nLeaf);
    NodeContainer rightNodes;
    rightNodes.Create(nLeaf);
    NodeContainer routers;
    routers.Create(2);

    // Connect left leaves to left router
    NetDeviceContainer leftDevices;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer pair(leftNodes.Get(i), routers.Get(0));
        NetDeviceContainer dev = leafLink.Install(pair);
        leftDevices.Add(dev.Get(0));
    }

    // Connect right leaves to right router
    NetDeviceContainer rightDevices;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer pair(rightNodes.Get(i), routers.Get(1));
        NetDeviceContainer dev = leafLink.Install(pair);
        rightDevices.Add(dev.Get(0));
    }

    // Inter-router bottleneck
    NetDeviceContainer routerDevices = bottleneck.Install(NodeContainer(routers.Get(0), routers.Get(1)));

    // Install stacks
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Assign addresses
    Ipv4AddressHelper addr;
    std::vector<Ipv4InterfaceContainer> leftInt, rightInt;

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        addr.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        Ipv4InterfaceContainer iic = addr.Assign(NetDeviceContainer(leftDevices.Get(i), routers.Get(0)->GetDevice(i)));
        leftInt.push_back(iic);
    }

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i+1 << ".0";
        addr.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        Ipv4InterfaceContainer iic = addr.Assign(NetDeviceContainer(rightDevices.Get(i), routers.Get(1)->GetDevice(i)));
        rightInt.push_back(iic);
    }

    addr.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckIfs = addr.Assign(routerDevices);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic Control
    TrafficControlHelper tch;
    std::string redType = (queueType == "RED") ? "ns3::RedQueueDisc" : "ns3::NlRedQueueDisc";

    Config::SetDefault("ns3::RedQueueDisc::Mode", EnumValue(byteMode ? RedQueueDisc::QUEUE_MODE_BYTES : RedQueueDisc::QUEUE_MODE_PACKETS));
    Config::SetDefault("ns3::NlRedQueueDisc::Mode", EnumValue(byteMode ? RedQueueDisc::QUEUE_MODE_BYTES : RedQueueDisc::QUEUE_MODE_PACKETS));
    Config::SetDefault("ns3::RedQueueDisc::QueueLimit",
                       UintegerValue(redQDiscLimit));
    Config::SetDefault("ns3::NlRedQueueDisc::QueueLimit",
                       UintegerValue(redQDiscLimit));
    Config::SetDefault("ns3::RedQueueDisc::MinTh",
                       DoubleValue(byteMode ? double(minTh * packSize) : double(minTh)));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh",
                       DoubleValue(byteMode ? double(maxTh * packSize) : double(maxTh)));
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(redQW));
    Config::SetDefault("ns3::NlRedQueueDisc::MinTh",
                       DoubleValue(byteMode ? double(minTh * packSize) : double(minTh)));
    Config::SetDefault("ns3::NlRedQueueDisc::MaxTh",
                       DoubleValue(byteMode ? double(maxTh * packSize) : double(maxTh)));
    Config::SetDefault("ns3::NlRedQueueDisc::QW", DoubleValue(redQW));

    tch.SetRootQueueDisc(redType);

    // Install RED/NLRED only on router 0's bottleneck device (facing router 1)
    Ptr<NetDevice> bottleneckDev = routerDevices.Get(0);
    Ptr<Node> router0 = routers.Get(0);

    QueueDiscContainer qdiscs = tch.Install( NetDeviceContainer(bottleneckDev) );
    Ptr<QueueDisc> queue = qdiscs.Get(0);

    // OnOff on right leaves, send to left leaves
    ApplicationContainer apps;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        Ipv4Address dstAddr = leftInt[i].GetAddress(0);
        uint16_t port = 10000 + i;

        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(dstAddr, port));
        onoff.SetAttribute("PacketSize", UintegerValue(packSize));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
        // Randomize On/Off times: Uniform [0.5, 2.0]s
        Ptr<UniformRandomVariable> onRv = CreateObject<UniformRandomVariable>();
        onRv->SetAttribute("Min", DoubleValue(0.5));
        onRv->SetAttribute("Max", DoubleValue(2.0));
        Ptr<UniformRandomVariable> offRv = CreateObject<UniformRandomVariable>();
        offRv->SetAttribute("Min", DoubleValue(0.5));
        offRv->SetAttribute("Max", DoubleValue(2.0));
        onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"));

        ApplicationContainer app = onoff.Install(rightNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(21.0));
        apps.Add(app);

        // Install UDP receiver on left node
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(leftNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(22.0));
        apps.Add(sinkApp);
    }

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();

    // Fetch and report stats
    uint32_t nTotalDrop = 0;
    uint32_t nMarkDrop = 0;
    uint32_t nForcedDrop = 0;
    if (queue)
    {
        QueueDisc::Stats st = queue->GetStats();
        nTotalDrop = st.nTotalDroppedPackets;
        nMarkDrop = st.nMarkDroppedPackets;
        nForcedDrop = st.nForcedDroppedPackets;
    }

    std::cout << "Queue Type        : " << queueType << std::endl;
    std::cout << "Drop Mode         : " << (byteMode ? "BYTES" : "PACKETS") << std::endl;
    std::cout << "RED/NLRED limit   : " << redQDiscLimit << std::endl;
    std::cout << "minTh / maxTh     : " << minTh << " / " << maxTh << std::endl;
    std::cout << "Total Drops       : " << nTotalDrop << std::endl;
    std::cout << "Mark Drops        : " << nMarkDrop << std::endl;
    std::cout << "Forced Drops      : " << nForcedDrop << std::endl;

    Simulator::Destroy();
    return 0;
}