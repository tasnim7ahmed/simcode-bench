#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellSimulation");

static void CwndChange(std::string context, uint32_t oldCwnd, uint32_t newCwnd) {
    std::ofstream outfile("congestion_window.txt", std::ios::app);
    outfile << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
    outfile.close();
}

static void QueueSizeChange(std::string context, uint32_t oldSize, uint32_t newSize) {
    std::ofstream outfile(context + "_queue_size.txt", std::ios::app);
    outfile << Simulator::Now().GetSeconds() << " " << newSize << std::endl;
    outfile.close();
}

void experiment(std::string qdiscType) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

    uint32_t nSenders = 7;
    double simulationTime = 10.0;
    std::string bottleneckLinkDataRate = "10Mbps";
    std::string bottleneckLinkDelay = "20ms";
    uint16_t sinkPort = 8080;
    uint16_t udpSinkPort = 9000;
    uint32_t maxBytes = 0;

    NodeContainer leftNodes;
    leftNodes.Create(nSenders);

    NodeContainer rightNodes;
    rightNodes.Create(1);

    NodeContainer routerNodes;
    routerNodes.Create(2);

    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPointLeaf.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer leftDevices = pointToPointLeaf.Install(leftNodes, routerNodes.Get(0));
    NetDeviceContainer rightDevices = pointToPointLeaf.Install(routerNodes.Get(1), rightNodes);

    PointToPointHelper pointToPointBottleneck;
    pointToPointBottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckLinkDataRate));
    pointToPointBottleneck.SetChannelAttribute("Delay", StringValue(bottleneckLinkDelay));

    NetDeviceContainer bottleneckDevices = pointToPointBottleneck.Install(routerNodes);

    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routerNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces = address.Assign(leftDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces = address.Assign(rightDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure Queue Disc
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(qdiscType);

    QueueDiscContainer queueDiscs;
    queueDiscs.Add(tch.Install(bottleneckDevices.Get(0)));
    queueDiscs.Add(tch.Install(bottleneckDevices.Get(1)));

    TypeId tid = TypeId::LookupByName("ns3::TcpNewReno");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(tid));

    // TCP traffic
    for (uint32_t i = 0; i < nSenders; ++i) {
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(0), sinkPort));
        ApplicationContainer sinkApp = sinkHelper.Install(rightNodes.Get(0));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(simulationTime));

        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(0), sinkPort));
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
        ApplicationContainer sourceApp = sourceHelper.Install(leftNodes.Get(i));
        sourceApp.Start(Seconds(2.0));
        sourceApp.Stop(Seconds(simulationTime));

        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/$ns3::TcpL4Protocol/Socket[0]/CongestionWindow", MakeCallback(&CwndChange));
    }

    // UDP traffic
    UdpServerHelper udpSink(udpSinkPort);
    ApplicationContainer sinkUdpApp = udpSink.Install(rightNodes.Get(0));
    sinkUdpApp.Start(Seconds(1.0));
    sinkUdpApp.Stop(Seconds(simulationTime));

    UdpClientHelper echoClient(rightInterfaces.GetAddress(0), udpSinkPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(leftNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Queue size tracing
    for (uint32_t i = 0; i < queueDiscs.GetNQueueDiscs(); ++i) {
        std::string queueDiscPath = "/NodeList/" + std::to_string(routerNodes.Get(i)->GetId()) +
                                    "/DeviceList/" + std::to_string(bottleneckDevices.Get(i)->GetIfIndex()) +
                                    "/$ns3::PointToPointNetDevice/TrafficControl/$ns3::QueueDisc/" + qdiscType + "/Queue/Size";
        std::string context = "queue" + std::to_string(i);
        Config::ConnectWithoutContext(queueDiscPath, MakeBoundCallback(&QueueSizeChange, context));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char* argv[]) {
    experiment("CoDelQueueDisc");
    experiment("CobaltQueueDisc");

    return 0;
}