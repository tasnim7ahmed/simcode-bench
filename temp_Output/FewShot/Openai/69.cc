#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-disc.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellRedFengAdaptiveRedExample");

int main(int argc, char *argv[])
{
    uint32_t nLeaf = 4;
    std::string queueType = "RED"; // "RED" or "FengAdaptiveRED"
    uint32_t queueLimitPackets = 100;
    uint32_t packetSize = 1024; // bytes
    std::string bottleneckBandwidth = "10Mbps";
    std::string bottleneckDelay = "10ms";
    std::string leafBandwidth = "100Mbps";
    std::string leafDelay = "1ms";
    uint32_t maxPackets = 100;
    double minTh = 5.0;
    double maxTh = 15.0;
    double maxP = 0.02;
    double qW = 0.002;
    double gentle = 1.0;

    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of leaf nodes on each side", nLeaf);
    cmd.AddValue("queueType", "Queue type: RED or FengAdaptiveRED", queueType);
    cmd.AddValue("queueLimitPackets", "Max packets in the queue", queueLimitPackets);
    cmd.AddValue("packetSize", "Application packet size (bytes)", packetSize);
    cmd.AddValue("bottleneckBandwidth", "Bandwidth of the bottleneck link", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Delay of the bottleneck link", bottleneckDelay);
    cmd.AddValue("leafBandwidth", "Bandwidth of access links", leafBandwidth);
    cmd.AddValue("leafDelay", "Delay of access links", leafDelay);
    cmd.AddValue("minTh", "RED Min threshold (packets)", minTh);
    cmd.AddValue("maxTh", "RED Max threshold (packets)", maxTh);
    cmd.AddValue("maxP", "RED MaxP parameter", maxP);
    cmd.AddValue("qW", "RED queue weight", qW);
    cmd.AddValue("gentle", "RED gentle mode", gentle);
    cmd.Parse(argc, argv);

    // Create dumbbell topology
    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create(nLeaf);
    rightNodes.Create(nLeaf);
    routers.Create(2);

    // Access links: left
    PointToPointHelper p2pLeaf;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue(leafBandwidth));
    p2pLeaf.SetChannelAttribute("Delay", StringValue(leafDelay));
    NetDeviceContainer leftDevices, routerLeftDevices;
    for (uint32_t i = 0; i < nLeaf; ++i) {
        NetDeviceContainer link = p2pLeaf.Install(leftNodes.Get(i), routers.Get(0));
        leftDevices.Add(link.Get(0));
        routerLeftDevices.Add(link.Get(1));
    }

    // Access links: right
    NetDeviceContainer rightDevices, routerRightDevices;
    for (uint32_t i = 0; i < nLeaf; ++i) {
        NetDeviceContainer link = p2pLeaf.Install(rightNodes.Get(i), routers.Get(1));
        rightDevices.Add(link.Get(0));
        routerRightDevices.Add(link.Get(1));
    }

    // Bottleneck link
    PointToPointHelper p2pRouter;
    p2pRouter.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    p2pRouter.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    NetDeviceContainer routerDevices = p2pRouter.Install(routers);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftInterfaces;
    for (uint32_t i = 0; i < nLeaf; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(NetDeviceContainer(leftDevices.Get(i), routerLeftDevices.Get(i)));
        leftInterfaces.push_back(iface);
    }

    std::vector<Ipv4InterfaceContainer> rightInterfaces;
    for (uint32_t i = 0; i < nLeaf; ++i) {
        std::ostringstream subnet;
        subnet << "10.2." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(NetDeviceContainer(rightDevices.Get(i), routerRightDevices.Get(i)));
        rightInterfaces.push_back(iface);
    }

    // Bottleneck link
    address.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIfaces = address.Assign(routerDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Queue Disc Setup on bottleneck (router 0 to router 1)
    TrafficControlHelper tch;
    if (queueType == "RED") {
        uint16_t handle = tch.SetRootQueueDisc("ns3::RedQueueDisc");
        tch.SetQueueDiscAttribute("MaxSize", QueueSizeValue(QueueSize(std::to_string(queueLimitPackets) + "p")));
        tch.SetQueueDiscAttribute("MinTh", DoubleValue(minTh));
        tch.SetQueueDiscAttribute("MaxTh", DoubleValue(maxTh));
        tch.SetQueueDiscAttribute("MaxP", DoubleValue(maxP));
        tch.SetQueueDiscAttribute("QW", DoubleValue(qW));
        tch.SetQueueDiscAttribute("Gentle", BooleanValue(gentle != 0.0));
    } else if (queueType == "FengAdaptiveRED") {
        uint16_t handle = tch.SetRootQueueDisc("ns3::FengAdaptiveRedQueueDisc");
        tch.SetQueueDiscAttribute("MaxSize", QueueSizeValue(QueueSize(std::to_string(queueLimitPackets) + "p")));
        tch.SetQueueDiscAttribute("MinTh", DoubleValue(minTh));
        tch.SetQueueDiscAttribute("MaxTh", DoubleValue(maxTh));
        tch.SetQueueDiscAttribute("MaxP", DoubleValue(maxP));
        tch.SetQueueDiscAttribute("QW", DoubleValue(qW));
        tch.SetQueueDiscAttribute("Gentle", BooleanValue(gentle != 0.0));
    } else {
        NS_ABORT_MSG("Unknown queue type " << queueType);
    }
    QueueDiscContainer qdiscs = tch.Install(routerDevices.Get(0));

    // Install TCP applications
    uint16_t basePort = 50000;
    ApplicationContainer serverApps, clientApps;

    for (uint32_t i = 0; i < nLeaf; ++i) {
        Address sinkLocalAddress(InetSocketAddress(rightInterfaces[i].GetAddress(0), basePort + i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        serverApps.Add(packetSinkHelper.Install(rightNodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(22.0));

    Ptr<UniformRandomVariable> onTimeRv = CreateObject<UniformRandomVariable>();
    Ptr<UniformRandomVariable> offTimeRv = CreateObject<UniformRandomVariable>();

    for (uint32_t i = 0; i < nLeaf; ++i) {
        OnOffHelper onoff("ns3::TcpSocketFactory", Address());
        AddressValue remoteAddress(
            InetSocketAddress(rightInterfaces[i].GetAddress(0), basePort + i));
        onoff.SetAttribute("Remote", remoteAddress);
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(leafBandwidth)));
        onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"));
        clientApps.Add(onoff.Install(leftNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(21.0));

    // Enable queue disc and packet drop logging
    // (Lost packets are counted by the queue disc via its statistics)

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();

    // Report queue disc statistics
    Ptr<QueueDisc> queue = qdiscs.Get(0);
    std::cout << "Queue disc type: " << queue->GetInstanceTypeId().GetName() << std::endl;
    std::cout << "    Packets enqueued: " << queue->GetStats().nPacketsEnqueued << std::endl;
    std::cout << "    Packets dequeued: " << queue->GetStats().nPacketsDequeued << std::endl;
    std::cout << "    Packets dropped:  " << queue->GetStats().nPacketsDropped << std::endl;
    std::cout << "    Bytes dropped:    " << queue->GetStats().nBytesDropped << std::endl;

    if (queue->GetStats().nPacketsDropped > 0) {
        std::cout << "Packet drops occurred as expected at the bottleneck queue." << std::endl;
    } else {
        std::cout << "No packet drops recorded -- check configuration (or try a higher offered load or lower queue limit)." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}