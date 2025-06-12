#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellSimulation");

void experiment(std::string queueDiscType);

int main(int argc, char *argv[]) {
    std::string tcpCwndFile = "tcp-cwnd-trace.txt";
    std::string cobaltQueueFile = "cobalt-queue-size.txt";
    std::string codelQueueFile = "codel-queue-size.txt";
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Run with COBALT
    experiment("ns3::CobaltQueueDisc");
    
    // Run with CoDel
    experiment("ns3::CoDelQueueDisc");

    return 0;
}

void experiment(std::string queueDiscType) {
    uint32_t nSenders = 7;
    NodeContainer senders, receivers;
    senders.Create(nSenders);
    receivers.Create(1);

    NodeContainer leftRouter, rightRouter;
    leftRouter.Create(1);
    rightRouter.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer senderDevices, routerLeftDevices, routerRightDevices, receiverDevices;

    for (uint32_t i = 0; i < nSenders; ++i) {
        NetDeviceContainer devices = p2p.Install(senders.Get(i), leftRouter.Get(0));
        senderDevices.Add(devices.Get(1));
        routerLeftDevices.Add(devices.Get(0));
    }

    NetDeviceContainer middleLink = p2p.Install(leftRouter.Get(0), rightRouter.Get(0));
    routerLeftDevices.Add(middleLink.Get(0));
    routerRightDevices.Add(middleLink.Get(1));

    NetDeviceContainer receiverLink = p2p.Install(rightRouter.Get(0), receivers.Get(0));
    routerRightDevices.Add(receiverLink.Get(0));
    receiverDevices.Add(receiverLink.Get(1));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer senderInterfaces, routerLeftInterfaces, routerRightInterfaces, receiverInterface;

    for (uint32_t i = 0; i < nSenders; ++i) {
        Ipv4InterfaceContainer ifc = address.Assign(NodeContainer(senders.Get(i), leftRouter.Get(0)));
        senderInterfaces.Add(ifc.Get(1));
        routerLeftInterfaces.Add(ifc.Get(0));
        address.NewNetwork();
    }

    Ipv4InterfaceContainer middleIfc = address.Assign(middleLink);
    routerLeftInterfaces.Add(middleIfc.Get(0));
    routerRightInterfaces.Add(middleIfc.Get(1));
    address.NewNetwork();

    Ipv4InterfaceContainer receiverIfc = address.Assign(receiverLink);
    routerRightInterfaces.Add(receiverIfc.Get(0));
    receiverInterface.Add(receiverIfc.Get(1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic Control Configuration
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueDiscType);
    QueueDiscContainer qdiscs = tch.Install(routerLeftDevices);

    // TCP traffic from one sender
    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(receivers.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(receiverInterface.GetAddress(0), sinkPort));
    onoff.SetConstantRate(DataRate("1Mbps"), 512);
    ApplicationContainer senderApp = onoff.Install(senders.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(9.0));

    // UDP traffic from remaining senders
    uint16_t udpPort = 9;
    UdpServerHelper server(udpPort);
    ApplicationContainer udpSinkApp = server.Install(receivers.Get(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(10.0));

    UdpClientHelper client;
    client.SetRemote(receiverInterface.GetAddress(0), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    for (uint32_t i = 1; i < nSenders; ++i) {
        ApplicationContainer app = client.Install(senders.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(9.0));
    }

    // Tracing congestion window for TCP
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("tcp-cwnd-trace.txt", std::ios::app);
    Config::Connect("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndChange, cwndStream));

    // Tracing queue size for each queue disc
    std::string queueFileName = (queueDiscType == "ns3::CobaltQueueDisc") ? "cobalt-queue-size.txt" : "codel-queue-size.txt";
    Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream(queueFileName);
    for (uint32_t i = 0; i < qdiscs.GetN(); ++i) {
        std::ostringstream oss;
        oss << "/NodeList/" << leftRouter.Get(0)->GetId() << "/TrafficControlLayer/RootQueueDisc/" << i << "/Enqueue";
        Config::Connect(oss.str(), MakeBoundCallback(&QueueSizeChange, queueStream));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
}