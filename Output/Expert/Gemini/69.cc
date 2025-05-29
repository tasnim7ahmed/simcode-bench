#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t nLeafNodes = 2;
    std::string queueType = "RED";
    uint32_t maxPackets = 20;
    uint32_t packetSize = 512;
    std::string dataRate = "10Mbps";
    std::string linkDelay = "2ms";
    std::string redMinTh = "10p";
    std::string redMaxTh = "30p";
    double redMarkProb = 0.02;
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("nLeafNodes", "Number of leaf nodes", nLeafNodes);
    cmd.AddValue("queueType", "Queue type (RED or ARE)", queueType);
    cmd.AddValue("maxPackets", "Max Packets in Queue", maxPackets);
    cmd.AddValue("packetSize", "Packet Size", packetSize);
    cmd.AddValue("dataRate", "Data Rate", dataRate);
    cmd.AddValue("linkDelay", "Link Delay", linkDelay);
    cmd.AddValue("redMinTh", "RED Min Threshold", redMinTh);
    cmd.AddValue("redMaxTh", "RED Max Threshold", redMaxTh);
    cmd.AddValue("redMarkProb", "RED Mark Probability", redMarkProb);
    cmd.AddValue("simulationTime", "Simulation Time", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    NodeContainer leftLeafNodes, rightLeafNodes, routerNodes;
    leftLeafNodes.Create(nLeafNodes);
    rightLeafNodes.Create(nLeafNodes);
    routerNodes.Create(2);

    NodeContainer allNodes;
    allNodes.Add(leftLeafNodes);
    allNodes.Add(rightLeafNodes);
    allNodes.Add(routerNodes);

    InternetStackHelper internet;
    internet.Install(allNodes);

    PointToPointHelper p2pLeaf;
    p2pLeaf.DataRate(dataRate);
    p2pLeaf.Delay(linkDelay);

    PointToPointHelper p2pRouter;
    p2pRouter.DataRate("50Mbps");
    p2pRouter.Delay("1ms");

    NetDeviceContainer leafRouterDevicesLeft[nLeafNodes];
    NetDeviceContainer leafRouterDevicesRight[nLeafNodes];
    NetDeviceContainer routerRouterDevices;

    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        leafRouterDevicesLeft[i] = p2pLeaf.Install(leftLeafNodes.Get(i), routerNodes.Get(0));
        leafRouterDevicesRight[i] = p2pLeaf.Install(rightLeafNodes.Get(i), routerNodes.Get(1));
    }
    routerRouterDevices = p2pRouter.Install(routerNodes.Get(0), routerNodes.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leafRouterIfacesLeft[nLeafNodes];
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        leafRouterIfacesLeft[i] = address.Assign(leafRouterDevicesLeft[i]);
    }

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer leafRouterIfacesRight[nLeafNodes];
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        leafRouterIfacesRight[i] = address.Assign(leafRouterDevicesRight[i]);
    }

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer routerRouterIfaces = address.Assign(routerRouterDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    TrafficControlLayer::GetInstance()->SetRootQueueDiscContainer(allNodes);

    for (uint32_t i = 0; i < 2; ++i) {
        Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
        em->SetAttribute("ErrorRate", DoubleValue(0.00001));
        em->SetAttribute("RandomVariable", PointerValue(CreateObject<UniformRandomVariable>()));
        routerRouterDevices.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    TypeId queueTypeId;
    if (queueType == "RED") {
        queueTypeId = TypeId::LookupByName("ns3::RedQueueDisc");
    } else if (queueType == "ARE") {
        queueTypeId = TypeId::LookupByName("ns3::AdaptiveRedQueueDisc");
    } else {
        std::cerr << "Invalid queue type: " << queueType << std::endl;
        return 1;
    }

    QueueDiscContainer queueDiscs;
    for (uint32_t i = 0; i < 2; ++i) {
        Ptr<QueueDisc> queueDisc = CreateObjectWithAttributes<QueueDisc>(queueTypeId,
                                                                         {{"LinkCapacity", StringValue("50Mbps")},
                                                                          {"MinTh", StringValue(redMinTh)},
                                                                          {"MaxTh", StringValue(redMaxTh)},
                                                                          {"MarkProb", DoubleValue(redMarkProb)},
                                                                          {"MaxSize", StringValue(std::to_string(maxPackets) + "p")}});
        TrafficControlHelper::SetRootQueueDisc(routerNodes.Get(i), queueDisc);
        queueDiscs.Add(queueDisc);
    }

    OnOffHelper onoff("ns3::TcpSocketFactory", Address(InetSocketAddress(routerRouterIfaces.GetAddress(1), 5000)));
    onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.1|Max=0.5]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", StringValue("40Mbps"));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        clientApps.Add(onoff.Install(leftLeafNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 5000));
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < nLeafNodes; ++i) {
        sinkApps.Add(sink.Install(rightLeafNodes.Get(i)));
    }

    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    uint64_t totalLost = 0;
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == routerRouterIfaces.GetAddress(1)) {
            totalLost += i->second.lostPackets;
        }
    }

    Simulator::Destroy();

    Ptr<QueueDisc> qd = TrafficControlHelper::GetRootQueueDisc(routerNodes.Get(0));
    if (qd != nullptr) {
        std::cout << "Queue Disc Statistics for Router 0: " << std::endl;
        qd->PrintStatistics(std::cout);
    } else {
        std::cout << "No Queue Disc found on Router 0" << std::endl;
    }

     qd = TrafficControlHelper::GetRootQueueDisc(routerNodes.Get(1));
    if (qd != nullptr) {
        std::cout << "Queue Disc Statistics for Router 1: " << std::endl;
        qd->PrintStatistics(std::cout);
    } else {
        std::cout << "No Queue Disc found on Router 1" << std::endl;
    }

    std::cout << "Total Lost Packets: " << totalLost << std::endl;

    return 0;
}