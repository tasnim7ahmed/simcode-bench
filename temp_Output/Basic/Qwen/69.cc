#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t nLeaf = 2;
    std::string queueType = "Red";
    uint32_t minTh = 5;
    uint32_t maxTh = 15;
    double queueSizePackets = 100;
    std::string dataRate = "1Mbps";
    std::string delay = "20ms";
    uint32_t packetSize = 1000;
    double startTime = 0.0;
    double stopTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nLeaf", "Number of leaf nodes on each side of the dumbbell", nLeaf);
    cmd.AddValue("queueType", "Queue disc type: Red or FengAdaptiveRed", queueType);
    cmd.AddValue("minTh", "RED minimum threshold (packets)", minTh);
    cmd.AddValue("maxTh", "RED maximum threshold (packets)", maxTh);
    cmd.AddValue("queueSize", "Queue size in packets", queueSizePackets);
    cmd.AddValue("dataRate", "Link data rate (e.g., 1Mbps)", dataRate);
    cmd.AddValue("delay", "Link delay (e.g., 20ms)", delay);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("startTime", "Application start time", startTime);
    cmd.AddValue("stopTime", "Simulation stop time", stopTime);
    cmd.Parse(argc, argv);

    NodeContainer leftLeafNodes;
    leftLeafNodes.Create(nLeaf);

    NodeContainer rightLeafNodes;
    rightLeafNodes.Create(nLeaf);

    NodeContainer routerLeft = NodeContainer(leftLeafNodes.Get(0));
    for (uint32_t i = 1; i < nLeaf; ++i) {
        routerLeft.Add(leftLeafNodes.Get(i));
    }

    NodeContainer routerRight = NodeContainer(rightLeafNodes.Get(0));
    for (uint32_t i = 1; i < nLeaf; ++i) {
        routerRight.Add(rightLeafNodes.Get(i));
    }

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer routerDevicesLeft, routerDevicesRight;
    std::vector<NetDeviceContainer> leafDevicesLeft(nLeaf), leafDevicesRight(nLeaf);

    for (uint32_t i = 0; i < nLeaf; ++i) {
        leafDevicesLeft[i] = p2p.Install(NodeContainer(leftLeafNodes.Get(i), routerLeft.Get(i)));
        leafDevicesRight[i] = p2p.Install(NodeContainer(routerRight.Get(i), rightLeafNodes.Get(i)));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    std::vector<Ipv4InterfaceContainer> ipInterfacesLeft(nLeaf), ipInterfacesRight(nLeaf);

    for (uint32_t i = 0; i < nLeaf; ++i) {
        ipInterfacesLeft[i] = address.Assign(leafDevicesLeft[i]);
        address.NewNetwork();
        ipInterfacesRight[i] = address.Assign(leafDevicesRight[i]);
        address.NewNetwork();
    }

    TrafficControlHelper tch;
    if (queueType == "Red") {
        tch.SetRootQueueDisc("ns3::RedQueueDisc",
                             "MinTh", UintegerValue(minTh),
                             "MaxTh", UintegerValue(maxTh),
                             "QueueLimit", UintegerValue(queueSizePackets));
    } else if (queueType == "FengAdaptiveRed") {
        tch.SetRootQueueDisc("ns3::FengAdaptiveRedQueueDisc",
                             "MinTh", UintegerValue(minTh),
                             "MaxTh", UintegerValue(maxTh),
                             "QueueLimit", UintegerValue(queueSizePackets));
    } else {
        NS_FATAL_ERROR("Unknown queue type: " << queueType);
    }

    QueueDiscContainer qdiscs = tch.Install(routerLeft.Get(0)->GetDevice(0));
    for (uint32_t i = 1; i < nLeaf; ++i) {
        qdiscs.Add(tch.Install(routerLeft.Get(i)->GetDevice(0)));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UniformVariableImpl uniform(0.1, 0.5);

    for (uint32_t i = 0; i < nLeaf; ++i) {
        Ptr<Node> receiverNode = rightLeafNodes.Get(i);
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), 80));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(receiverNode);
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(stopTime));

        OnOffHelper client("ns3::TcpSocketFactory", Address());
        client.SetAttribute("Remote", AddressValue(InetSocketAddress(ipInterfacesRight[i].GetAddress(1), 80)));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=0.5]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApp = client.Install(leftLeafNodes.Get(i));
        clientApp.Start(Seconds(startTime));
        clientApp.Stop(Seconds(stopTime));
    }

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();

    for (uint32_t i = 0; i < nLeaf; ++i) {
        Ptr<QueueDisc> q = StaticCast<PointToPointNetDevice>(routerLeft.Get(i)->GetDevice(0))->GetQueueDisc();
        QueueDisc::Stats stats = q->GetStats();
        std::cout << "Router " << i << " queue disc statistics:" << std::endl;
        std::cout << "  Total dropped packets: " << stats.GetNDroppedPackets() << std::endl;
        std::cout << "  Total enqueues: " << stats.GetNEnqueues() << std::endl;
        std::cout << "  Total dequeues: " << stats.GetNDequeues() << std::endl;
    }

    Simulator::Destroy();
    return 0;
}