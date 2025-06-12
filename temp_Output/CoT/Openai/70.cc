#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/stats-module.h"
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueDiscExample");

void PrintQueueDiscStats(Ptr<QueueDisc> q)
{
    QueueDisc::Stats stats = q->GetStats();
    std::cout << "QueueDisc Stats:" << std::endl;
    std::cout << "  Packets dropped by QueueDisc: " << stats.nTotalDroppedPackets << std::endl;
    std::cout << "  Bytes dropped by QueueDisc: " << stats.nTotalDroppedBytes << std::endl;
    std::cout << "  Pcaket dropped by Queue: " << stats.nTotalMarkedPackets << std::endl;
    std::cout << "  Bytes dropped by Queue: " << stats.nTotalMarkedBytes << std::endl;
}

int main(int argc, char *argv[])
{
    uint32_t nLeaf = 4;
    std::string bottleneckRate = "10Mbps";
    std::string bottleneckDelay = "5ms";
    std::string accessRate = "100Mbps";
    std::string accessDelay = "1ms";
    std::string queueDiscType = "RED";
    uint32_t pktSize = 1000; // bytes
    uint32_t maxPackets = 1000;
    uint32_t maxBytes = 1000000;
    bool useByteMode = false;
    double minTh = 5;
    double maxTh = 15;
    double qW = 0.002;
    double lInter = 500.0; // ms for On time mean
    double lOff = 500.0; // ms for Off time mean
    double totalTime = 20.0; // seconds

    CommandLine cmd;
    cmd.AddValue("nLeaf", "Number of left and right leaf nodes", nLeaf);
    cmd.AddValue("pktSize", "Packet size in bytes", pktSize);
    cmd.AddValue("bottleneckRate", "Bottleneck link data rate", bottleneckRate);
    cmd.AddValue("bottleneckDelay", "Bottleneck link propagation delay", bottleneckDelay);
    cmd.AddValue("accessRate", "Access link data rate", accessRate);
    cmd.AddValue("accessDelay", "Access link propagation delay", accessDelay);
    cmd.AddValue("queueDiscType", "RED or NLRED", queueDiscType);
    cmd.AddValue("maxPackets", "Max packets for RED/NLRED", maxPackets);
    cmd.AddValue("maxBytes", "Max bytes for RED/NLRED", maxBytes);
    cmd.AddValue("minTh", "RED/NLRED minimum threshold (in packets or bytes)", minTh);
    cmd.AddValue("maxTh", "RED/NLRED maximum threshold (in packets or bytes)", maxTh);
    cmd.AddValue("qW", "RED/NLRED queue weight (qW) parameter", qW);
    cmd.AddValue("useByteMode", "Set true for byte mode, false for packet mode", useByteMode);
    cmd.AddValue("lInter", "Mean OnTime (milliseconds) for OnOff", lInter);
    cmd.AddValue("lOff", "Mean OffTime (milliseconds) for OnOff", lOff);
    cmd.AddValue("totalTime", "Duration of simulation in seconds", totalTime);
    cmd.Parse(argc, argv);

    std::string qdiscType = queueDiscType;
    std::transform(qdiscType.begin(), qdiscType.end(), qdiscType.begin(), ::toupper);
    if (qdiscType != "RED" && qdiscType != "NLRED") {
        NS_ABORT_MSG("Invalid queueDiscType: use RED or NLRED");
    }

    NodeContainer leftLeaf, rightLeaf, routers;
    leftLeaf.Create(nLeaf);
    rightLeaf.Create(nLeaf);
    routers.Create(2);

    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue(accessRate));
    access.SetChannelAttribute("Delay", StringValue(accessDelay));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    bottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    // Connect left leafs to router 0
    NetDeviceContainer leftDevs;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer devs = access.Install(leftLeaf.Get(i), routers.Get(0));
        leftDevs.Add(devs.Get(0));
    }

    // Connect right leafs to router 1
    NetDeviceContainer rightDevs;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer devs = access.Install(rightLeaf.Get(i), routers.Get(1));
        rightDevs.Add(devs.Get(0));
    }

    // Connect routers
    NetDeviceContainer routerDevs = bottleneck.Install(routers);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(leftLeaf);
    internet.Install(rightLeaf);
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> leftInt, rightInt;

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(NetDeviceContainer(leftDevs.Get(i), routers.Get(0)->GetDevice(i+1)));
        leftInt.push_back(iface);
    }

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.2." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(NetDeviceContainer(rightDevs.Get(i), routers.Get(1)->GetDevice(i+1)));
        rightInt.push_back(iface);
    }

    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerIf = address.Assign(routerDevs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TrafficControl, on left router output to right router
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc(qdiscType == "RED" ? "ns3::RedQueueDisc" : "ns3::NlredQueueDisc");
    if (useByteMode) {
        // Byte mode
        tch.SetQueueDiscAttribute("Mode", StringValue("QUEUE_DISC_MODE_BYTES"));
        tch.SetQueueDiscAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, maxBytes)));
        tch.SetQueueDiscAttribute("MinTh", DoubleValue(minTh * pktSize));
        tch.SetQueueDiscAttribute("MaxTh", DoubleValue(maxTh * pktSize));
    }
    else {
        // Packet mode
        tch.SetQueueDiscAttribute("Mode", StringValue("QUEUE_DISC_MODE_PACKETS"));
        tch.SetQueueDiscAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, maxPackets)));
        tch.SetQueueDiscAttribute("MinTh", DoubleValue(minTh));
        tch.SetQueueDiscAttribute("MaxTh", DoubleValue(maxTh));
    }
    tch.SetQueueDiscAttribute("QW", DoubleValue(qW));

    // Find the output NetDevice on the left router (the bottleneck)
    Ptr<Node> leftRouter = routers.Get(0);
    Ptr<NetDevice> leftRouterBottleneckDev = routerDevs.Get(0);

    QueueDiscContainer qdiscs = tch.Install(leftRouterBottleneckDev);

    // Create and install OnOff apps on right nodes
    ApplicationContainer apps;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        Ptr<Node> src = rightLeaf.Get(i);
        Ptr<Node> dst = leftLeaf.Get(i);

        OnOffHelper onoff("ns3::UdpSocketFactory", 
            InetSocketAddress(leftInt[i].GetAddress(0), 5000));
        onoff.SetAttribute("PacketSize", UintegerValue(pktSize));
        onoff.SetAttribute("DataRate", StringValue("200Mbps"));

        Ptr<ExponentialRandomVariable> onVar = CreateObject<ExponentialRandomVariable>();
        Ptr<ExponentialRandomVariable> offVar = CreateObject<ExponentialRandomVariable>();
        onVar->SetAttribute("Mean", DoubleValue(lInter));
        offVar->SetAttribute("Mean", DoubleValue(lOff));
        onoff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=" + std::to_string(lInter/1000.0) + "]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=" + std::to_string(lOff/1000.0) + "]"));

        ApplicationContainer temp = onoff.Install(src);
        temp.Start(Seconds(1.0 + i*0.05));
        temp.Stop(Seconds(totalTime - 1.0));
        apps.Add(temp);

        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 5000));
        ApplicationContainer sinkApp = sink.Install(dst);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(totalTime));
        apps.Add(sinkApp);
    }

    Simulator::Stop(Seconds(totalTime));

    Simulator::Run();

    if (!qdiscs.IsEmpty()) {
        Ptr<QueueDisc> q = qdiscs.Get(0);
        if (q != nullptr) {
            PrintQueueDiscStats(q);
        }
        else {
            std::cout << "Warning: QueueDisc was nullptr" << std::endl;
        }
    }
    else {
        std::cout << "Warning: No QueueDisc was installed" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}