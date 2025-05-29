#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t nLeaf = 4;
    std::string queueType = "RED";
    uint32_t maxPackets = 100;
    uint32_t packetSize = 1024;
    std::string bottleneckBandwidth = "10Mbps";
    std::string bottleneckDelay = "10ms";
    std::string leafBandwidth = "100Mbps";
    std::string leafDelay = "2ms";
    double minTh = 5;
    double maxTh = 15;
    double qW = 0.002;
    double lInterm = 50;
    std::string dataRate = "5Mbps";
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of left and right leaf nodes", nLeaf);
    cmd.AddValue("queueType", "RED or FRED", queueType);
    cmd.AddValue("maxPackets", "Max number of packets allowed in queue", maxPackets);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("bottleneckBandwidth", "Bottleneck link bandwidth", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("leafBandwidth", "Access link bandwidth", leafBandwidth);
    cmd.AddValue("leafDelay", "Access link delay", leafDelay);
    cmd.AddValue("minTh", "RED min threshold (packets)", minTh);
    cmd.AddValue("maxTh", "RED max threshold (packets)", maxTh);
    cmd.AddValue("qW", "RED queue weight", qW);
    cmd.AddValue("lInterm", "RED lInterm", lInterm);
    cmd.AddValue("dataRate", "Flow data rate", dataRate);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    NodeContainer leftNodes, rightNodes;
    leftNodes.Create(nLeaf);
    rightNodes.Create(nLeaf);
    NodeContainer routers;
    routers.Create(2);

    // Left access links
    PointToPointHelper leftHelper;
    leftHelper.SetDeviceAttribute("DataRate", StringValue(leafBandwidth));
    leftHelper.SetChannelAttribute("Delay", StringValue(leafDelay));
    NetDeviceContainer leftDevs, routerLeftDevs;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer n(leftNodes.Get(i), routers.Get(0));
        NetDeviceContainer d = leftHelper.Install(n);
        leftDevs.Add(d.Get(0));
        routerLeftDevs.Add(d.Get(1));
    }

    // Right access links
    PointToPointHelper rightHelper;
    rightHelper.SetDeviceAttribute("DataRate", StringValue(leafBandwidth));
    rightHelper.SetChannelAttribute("Delay", StringValue(leafDelay));
    NetDeviceContainer rightDevs, routerRightDevs;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NodeContainer n(routers.Get(1), rightNodes.Get(i));
        NetDeviceContainer d = rightHelper.Install(n);
        routerRightDevs.Add(d.Get(0));
        rightDevs.Add(d.Get(1));
    }

    // Bottleneck link
    PointToPointHelper bottleneckHelper;
    bottleneckHelper.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    bottleneckHelper.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    NetDeviceContainer bottleneckDevs = bottleneckHelper.Install(routers);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Assign addresses
    Ipv4AddressHelper addr;
    std::vector<Ipv4InterfaceContainer> leftIfs(nLeaf), rightIfs(nLeaf);

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        addr.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        leftIfs[i] = addr.Assign(NetDeviceContainer(leftDevs.Get(i), routerLeftDevs.Get(i)));
    }

    addr.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckIfs = addr.Assign(bottleneckDevs);

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.3." << i+1 << ".0";
        addr.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        rightIfs[i] = addr.Assign(NetDeviceContainer(routerRightDevs.Get(i), rightDevs.Get(i)));
    }

    // Traffic Control on bottleneck
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc(
        queueType == "FRED" ?
            "ns3::FqCoDelQueueDisc" :
            "ns3::RedQueueDisc");
    Config::SetDefault("ns3::RedQueueDisc::MaxSize", QueueSizeValue(QueueSize(std::to_string(maxPackets) + "p")));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", DoubleValue(minTh));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", DoubleValue(maxTh));
    Config::SetDefault("ns3::RedQueueDisc::QW", DoubleValue(qW));
    Config::SetDefault("ns3::RedQueueDisc::LInterm", DoubleValue(lInterm));
    Ptr<QueueDisc> qDisc;
    if (queueType == "FRED")
    {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }
    else
    {
        tch.SetRootQueueDisc("ns3::RedQueueDisc");
    }
    QueueDiscContainer qdiscs = tch.Install(bottleneckDevs.Get(0));
    qDisc = qdiscs.Get(0);

    // Populate routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // OnOff Apps on all right-side nodes (sink on left)
    double startTime = 1.0;
    double stopTime = simulationTime;
    ApplicationContainer apps;
    Ptr<UniformRandomVariable> onTime = CreateObject<UniformRandomVariable>();
    onTime->SetAttribute("Min", DoubleValue(0.5));
    onTime->SetAttribute("Max", DoubleValue(1.5));
    Ptr<UniformRandomVariable> offTime = CreateObject<UniformRandomVariable>();
    offTime->SetAttribute("Min", DoubleValue(0.5));
    offTime->SetAttribute("Max", DoubleValue(1.5));

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        Address sinkAddress (InetSocketAddress(leftIfs[i].GetAddress(0), 5000));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 5000));
        ApplicationContainer sinkApp = sinkHelper.Install(leftNodes.Get(i));
        sinkApp.Start(Seconds(0));
        sinkApp.Stop(Seconds(simulationTime+2));

        OnOffHelper onOff("ns3::TcpSocketFactory", sinkAddress);
        onOff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        onOff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onOff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
        onOff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
        ApplicationContainer app = onOff.Install(rightNodes.Get(i));
        app.Start(Seconds(startTime));
        app.Stop(Seconds(stopTime));
        apps.Add(app);
    }

    Simulator::Stop(Seconds(simulationTime+1.0));

    Simulator::Run();

    // Print final queue disc statistics
    qDisc->GetStats().Print(std::cout);

    uint32_t drops = qDisc->GetStats().nTotalDropped;
    if (drops == 0)
    {
        std::cout << "WARNING: No packets were dropped!" << std::endl;
    }
    else
    {
        std::cout << "Packets dropped by queue disc: " << drops << std::endl;
    }

    Simulator::Destroy();
    return 0;
}