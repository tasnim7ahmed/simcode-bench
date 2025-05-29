#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellRedExample");

int main(int argc, char *argv[])
{
    // Default parameters
    uint32_t nLeaf = 4;
    std::string queueType = "RED"; // can be "RED" or "FengAdaptiveRED"
    uint32_t maxPackets = 100;
    uint32_t pktSize = 1000;
    std::string leafDataRate = "10Mbps";
    std::string leafDelay = "2ms";
    std::string bottleneckDataRate = "1Mbps";
    std::string bottleneckDelay = "10ms";
    double minTh = 5.0;
    double maxTh = 15.0;
    double qW = 0.002;

    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of leaf nodes on each side", nLeaf);
    cmd.AddValue("queueType", "QueueDisc type: RED or FengAdaptiveRED", queueType);
    cmd.AddValue("maxPackets", "Max packets allowed in queue", maxPackets);
    cmd.AddValue("pktSize", "Packet size in bytes", pktSize);
    cmd.AddValue("leafDataRate", "Leaf link bandwidth", leafDataRate);
    cmd.AddValue("leafDelay", "Leaf link delay", leafDelay);
    cmd.AddValue("bottleneckDataRate", "Bottleneck link bandwidth", bottleneckDataRate);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("minTh", "RED min threshold", minTh);
    cmd.AddValue("maxTh", "RED max threshold", maxTh);
    cmd.AddValue("qW", "RED queue weight", qW);
    cmd.Parse(argc, argv);

    if (queueType != "RED" && queueType != "FengAdaptiveRED")
    {
        NS_ABORT_MSG("queueType must be RED or FengAdaptiveRED");
    }

    // Create dumbbell topology
    PointToPointHelper p2pLeaf;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue(leafDataRate));
    p2pLeaf.SetChannelAttribute("Delay", StringValue(leafDelay));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NodeContainer leftNodes;
    leftNodes.Create(nLeaf);

    NodeContainer rightNodes;
    rightNodes.Create(nLeaf);

    NodeContainer routers;
    routers.Create(2);

    // Left links
    NetDeviceContainer leftDevs;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer pair(leftNodes.Get(i), routers.Get(0));
        NetDeviceContainer link = p2pLeaf.Install(pair);
        leftDevs.Add(link.Get(0));
    }

    // Right links
    NetDeviceContainer rightDevs;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer pair(rightNodes.Get(i), routers.Get(1));
        NetDeviceContainer link = p2pLeaf.Install(pair);
        rightDevs.Add(link.Get(0));
    }

    // Bottleneck link
    NetDeviceContainer bottleneckDevs = p2pBottleneck.Install(NodeContainer(routers.Get(0), routers.Get(1)));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftInt(nLeaf), rightInt(nLeaf);

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        NetDeviceContainer link;
        link.Add(leftDevs.Get(i));
        link.Add(routers.Get(0)->GetDevice(i));
        leftInt[i] = address.Assign(link);
    }

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        NetDeviceContainer link;
        link.Add(rightDevs.Get(i));
        link.Add(routers.Get(1)->GetDevice(i));
        rightInt[i] = address.Assign(link);
    }

    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIfs = address.Assign(bottleneckDevs);

    // Traffic Control (RED/FengAdaptiveRED) on bottleneck
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc(queueType == "RED" ? "ns3::RedQueueDisc" : "ns3::FengAdaptiveRedQueueDisc");
    tch.SetQueueDiscAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, maxPackets)));
    tch.SetQueueDiscAttribute("MinTh", DoubleValue(minTh));
    tch.SetQueueDiscAttribute("MaxTh", DoubleValue(maxTh));
    tch.SetQueueDiscAttribute("QW", DoubleValue(qW));
    QueueDiscContainer qdiscs = tch.Install(bottleneckDevs);

    // Assign default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications: Install TCP OnOff on right-side nodes (receiving on left nodes)
    uint16_t portBase = 9000;
    ApplicationContainer serverApps, clientApps;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        // PacketSink at left node
        Address sinkLocalAddress (InetSocketAddress(leftInt[i].GetAddress(0), portBase + i));
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
        serverApps.Add(sinkHelper.Install(leftNodes.Get(i)));

        // TCP OnOff on right node
        OnOffHelper onoff("ns3::TcpSocketFactory",
                          InetSocketAddress(leftInt[i].GetAddress(0), portBase + i));
        onoff.SetAttribute("PacketSize", UintegerValue(pktSize));
        onoff.SetAttribute("DataRate", StringValue(leafDataRate));
        onoff.SetAttribute("MaxBytes", UintegerValue(0));

        Ptr<UniformRandomVariable> onTime = CreateObject<UniformRandomVariable>();
        onTime->SetAttribute("Min", DoubleValue(0.5));
        onTime->SetAttribute("Max", DoubleValue(2.0));
        onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"));

        Ptr<UniformRandomVariable> offTime = CreateObject<UniformRandomVariable>();
        offTime->SetAttribute("Min", DoubleValue(0.1));
        offTime->SetAttribute("Max", DoubleValue(0.5));
        onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"));

        clientApps.Add(onoff.Install(rightNodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Output QueueDisc statistics
    Ptr<QueueDisc> q = qdiscs.Get(0);
    std::cout << "*** QueueDisc statistics at the bottleneck:" << std::endl;
    std::cout << " Packets in queue: " << q->GetNPackets() << std::endl;
    std::cout << " Bytes in queue: " << q->GetNBytes() << std::endl;
    std::cout << " Dropped packets (total): " << q->GetStats().nTotalDroppedPackets << std::endl;
    std::cout << " Dropped bytes (total): " << q->GetStats().nTotalDroppedBytes << std::endl;
    std::cout << " Marked packets (ECN): " << q->GetStats().nTotalMarkedPackets << std::endl;

    if (q->GetStats().nTotalDroppedPackets == 0)
    {
        std::cout << "WARNING: No packets were dropped -- network may be underutilized for chosen parameters." << std::endl;
    }
    Simulator::Destroy();
    return 0;
}