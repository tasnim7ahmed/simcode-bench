#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellRedFared");

int main(int argc, char *argv[])
{
    // Configurable parameters
    uint32_t nLeaf = 4;
    std::string queueType = "RED"; // "RED" or "FqCoDel" (ns-3 does not have FARED, but supports FqCoDel RED variant)
    uint32_t maxPackets = 100;     // Max packets allowed in queue
    uint32_t packetSize = 1024;    // bytes
    std::string dataRate = "10Mbps";
    std::string bottleneckRate = "2Mbps";
    std::string leafDelay = "2ms";
    std::string bottleneckDelay = "10ms";
    double minTh = 5;
    double maxTh = 15;
    double queueWeight = 0.002;
    bool useFared = false; // Default to RED

    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of left/right leaf nodes", nLeaf);
    cmd.AddValue("queueType", "Queue disc type: RED or FARED", queueType);
    cmd.AddValue("maxPackets", "Queue disc max packets", maxPackets);
    cmd.AddValue("packetSize", "Packet payload size in bytes", packetSize);
    cmd.AddValue("dataRate", "Data rate for leaf links", dataRate);
    cmd.AddValue("bottleneckRate", "Data rate for bottleneck link", bottleneckRate);
    cmd.AddValue("leafDelay", "Delay for leaf links", leafDelay);
    cmd.AddValue("bottleneckDelay", "Delay for bottleneck link", bottleneckDelay);
    cmd.AddValue("minTh", "RED MinTh threshold", minTh);
    cmd.AddValue("maxTh", "RED MaxTh threshold", maxTh);
    cmd.AddValue("queueWeight", "RED queue weight", queueWeight);
    cmd.Parse(argc, argv);

    if (queueType == "FARED" || queueType == "Fared" || queueType == "fared")
    {
        useFared = true;
        queueType = "RED"; // will enable adaptive mode below
    }

    // Create dumbbell topology
    NodeContainer leftNodes, rightNodes, routers;
    leftNodes.Create(nLeaf);
    rightNodes.Create(nLeaf);
    routers.Create(2);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // Point-to-point helpers
    PointToPointHelper p2pLeaf;
    p2pLeaf.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2pLeaf.SetChannelAttribute("Delay", StringValue(leafDelay));

    PointToPointHelper p2pRouter;
    p2pRouter.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    p2pRouter.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    // Install devices and create dumbbell links
    std::vector<NetDeviceContainer> leftDevices(nLeaf), rightDevices(nLeaf);
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        leftDevices[i] = p2pLeaf.Install(leftNodes.Get(i), routers.Get(0));
        rightDevices[i] = p2pLeaf.Install(rightNodes.Get(i), routers.Get(1));
    }
    NetDeviceContainer routerDevices = p2pRouter.Install(routers.Get(0), routers.Get(1));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> leftIntfs(nLeaf), rightIntfs(nLeaf);

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        leftIntfs[i] = ipv4.Assign(leftDevices[i]);
    }
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        rightIntfs[i] = ipv4.Assign(rightDevices[i]);
    }
    ipv4.SetBase("10.3.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIfs = ipv4.Assign(routerDevices);

    // Configure RED queue disc or FARED (Adaptive RED)
    TrafficControlHelper tch;
    if (queueType == "RED")
    {
        uint16_t handle = tch.SetRootQueueDisc("ns3::RedQueueDisc",
                                               "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, maxPackets)),
                                               "MinTh", DoubleValue(minTh),
                                               "MaxTh", DoubleValue(maxTh),
                                               "QueueWeight", DoubleValue(queueWeight),
                                               "LinkBandwidth", StringValue(bottleneckRate),
                                               "LinkDelay", StringValue(bottleneckDelay));
        if (useFared)
        {
            Config::Set("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Mode", StringValue("QUEUE_MODE_PACKETS"));
            tch.SetQueueDiscAttribute("AdaptMaxP", BooleanValue(true)); // Enable adaptive mode
            tch.SetQueueDiscAttribute("UseEcn", BooleanValue(false));
        }
    }
    // Install on the bottleneck router device (between two routers)
    tch.Install(routerDevices);

    // Assign IP addresses
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP OnOff applications on right-side nodes -> left-side nodes
    ApplicationContainer onOffApps;
    ApplicationContainer sinkApps;
    double startTime = 0.1;
    double stopTime = 20.0;
    uint16_t portBase = 5000;

    Ptr<UniformRandomVariable> onTimeVar = CreateObject<UniformRandomVariable>();
    Ptr<UniformRandomVariable> offTimeVar = CreateObject<UniformRandomVariable>();
    onTimeVar->SetAttribute("Min", DoubleValue(0.5));
    onTimeVar->SetAttribute("Max", DoubleValue(1.0));
    offTimeVar->SetAttribute("Min", DoubleValue(0.1));
    offTimeVar->SetAttribute("Max", DoubleValue(0.2));

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        Address sinkAddress(InetSocketAddress(leftIntfs[i].GetAddress(0), portBase + i));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(leftNodes.Get(i));
        sinkApps.Add(sinkApp);

        OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
        onoff.SetAttribute("OnTime", PointerValue(onTimeVar));
        onoff.SetAttribute("OffTime", PointerValue(offTimeVar));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("DataRate", StringValue(bottleneckRate));
        ApplicationContainer app = onoff.Install(rightNodes.Get(i));
        app.Start(Seconds(startTime));
        app.Stop(Seconds(stopTime));
        onOffApps.Add(app);

        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(stopTime + 1));
    }

    // Enable pcap on bottleneck link for reference
    p2pRouter.EnablePcapAll("dumbbell-red-bottleneck", false);

    Simulator::Stop(Seconds(stopTime + 1));

    // Run simulation
    Simulator::Run();

    // Print QueueDisc statistics for bottleneck
    Ptr<QueueDisc> queueDisc = DynamicCast<QueueDisc>(routerDevices.Get(0)->GetNode()->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(routerDevices.Get(0)));
    if (!queueDisc)
    {
        // Try both devices
        queueDisc = DynamicCast<QueueDisc>(routerDevices.Get(1)->GetNode()->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(routerDevices.Get(1)));
    }
    std::cout << "QueueDisc statistics at bottleneck link:" << std::endl;
    if (queueDisc)
    {
        queueDisc->GetStatistics().Print(std::cout);
        std::cout << "    Dropped packets: " << queueDisc->GetStats().nDrop << std::endl;
    }
    else
    {
        std::cout << "    Unable to get queue disc statistics!" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}