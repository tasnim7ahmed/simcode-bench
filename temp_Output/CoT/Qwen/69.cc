#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueSimulation");

int main(int argc, char *argv[]) {
    uint32_t leftLeafCount = 2;
    uint32_t rightLeafCount = 2;
    std::string queueType = "Red";
    uint32_t minTh = 5;
    uint32_t maxTh = 15;
    double queueLimit = 100;
    std::string linkRate = "1Mbps";
    std::string linkDelay = "10ms";
    uint32_t packetSize = 1000;
    bool useFengAdaptiveRed = false;
    bool enableAnimation = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("leftLeafCount", "Number of left leaf nodes", leftLeafCount);
    cmd.AddValue("rightLeafCount", "Number of right leaf nodes", rightLeafCount);
    cmd.AddValue("queueType", "Queue type: Red or FengAdaptiveRed", queueType);
    cmd.AddValue("minTh", "Minimum threshold for RED queue", minTh);
    cmd.AddValue("maxTh", "Maximum threshold for RED queue", maxTh);
    cmd.AddValue("queueLimit", "Queue limit", queueLimit);
    cmd.AddValue("linkRate", "Link data rate (e.g., 1Mbps)", linkRate);
    cmd.AddValue("linkDelay", "Link delay (e.g., 10ms)", linkDelay);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("useFengAdaptiveRed", "Use Feng's Adaptive RED instead of standard RED", useFengAdaptiveRed);
    cmd.AddValue("enableAnimation", "Enable NetAnim XML trace file output", enableAnimation);
    cmd.Parse(argc, argv);

    if (queueType == "FengAdaptiveRed") {
        useFengAdaptiveRed = true;
    }

    NodeContainer leftLeafNodes;
    leftLeafNodes.Create(leftLeafCount);

    NodeContainer rightLeafNodes;
    rightLeafNodes.Create(rightLeafCount);

    NodeContainer routerLeft, routerRight;
    routerLeft.Create(1);
    routerRight.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(linkRate));
    p2p.SetChannelAttribute("Delay", StringValue(linkDelay));

    NetDeviceContainer routerDevicesLeft;
    for (uint32_t i = 0; i < leftLeafCount; ++i) {
        NetDeviceContainer devices = p2p.Install(leftLeafNodes.Get(i), routerLeft.Get(0));
        routerDevicesLeft.Add(devices.Get(1));
    }

    NetDeviceContainer routerDevicesRight;
    for (uint32_t i = 0; i < rightLeafCount; ++i) {
        NetDeviceContainer devices = p2p.Install(routerRight.Get(0), rightLeafNodes.Get(i));
        routerDevicesRight.Add(devices.Get(0));
    }

    NetDeviceContainer centralLink = p2p.Install(routerLeft.Get(0), routerRight.Get(0));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer centralInterfaces;
    for (uint32_t i = 0; i < leftLeafCount; ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(p2p.GetNetDevices(leftLeafNodes.Get(i), routerLeft.Get(0)));
        address.NewNetwork();
    }

    for (uint32_t i = 0; i < rightLeafCount; ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(p2p.GetNetDevices(routerRight.Get(0), rightLeafNodes.Get(i)));
        address.NewNetwork();
    }

    centralInterfaces = address.Assign(centralLink);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    TrafficControlHelper tch;
    if (useFengAdaptiveRed) {
        tch.SetRootQueueDisc("ns3::AdaptiveRedQueueDisc",
                             "MinTh", UintegerValue(minTh),
                             "MaxTh", UintegerValue(maxTh),
                             "LinkBandwidth", DataRateValue(DataRate(linkRate)),
                             "LinkDelay", TimeValue(Time(linkDelay)),
                             "MaxSize", QueueSizeValue(QueueSize(" packets", queueLimit)));
    } else {
        tch.SetRootQueueDisc("ns3::RedQueueDisc",
                             "MinTh", UintegerValue(minTh),
                             "MaxTh", UintegerValue(maxTh),
                             "MaxSize", QueueSizeValue(QueueSize(" packets", queueLimit)));
    }

    QueueDiscContainer qdiscs = tch.Install(centralLink.Get(1));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(linkRate)));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < rightLeafCount; ++i) {
        UniformRandomVariable rand;
        double startTime = rand.GetValue(1.0, 3.0);
        double stopTime = rand.GetValue(startTime + 5.0, 10.0);
        Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), 9));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(rightLeafNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        AddressValue remoteAddress(InetSocketAddress(centralInterfaces.GetAddress(1), 9));
        onoff.SetAttribute("Remote", remoteAddress);
        apps = onoff.Install(leftLeafNodes.Get(i % leftLeafCount));
        apps.Start(Seconds(startTime));
        apps.Stop(Seconds(stopTime));
    }

    if (enableAnimation) {
        AnimationInterface anim("dumbbell-queue-sim.xml");
        anim.EnablePacketMetadata(true);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Ptr<QueueDisc> q = qdiscs.Get(0);
    std::cout << "Queue disc statistics:" << std::endl;
    std::cout << "  Total packets dropped: " << q->GetTotalDroppedPackets() << std::endl;
    std::cout << "  Total packets enqueued: " << q->GetTotalEnqueuedPackets() << std::endl;
    std::cout << "  Total packets dequeued: " << q->GetTotalDequeuedPackets() << std::endl;

    if (q->GetTotalDroppedPackets() > 0) {
        std::cout << "Packet drops occurred as expected." << std::endl;
    } else {
        std::cout << "No packet drops observed." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}