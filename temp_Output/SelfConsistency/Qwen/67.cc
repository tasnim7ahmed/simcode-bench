#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellTrafficControlExperiment");

void
experiment(std::string queueDiscType)
{
    // Create dumbbell topology: 7 senders, 1 router on left; 1 receiver, 1 router on right
    NodeContainer senders;
    senders.Create(7);

    NodeContainer leftRouter;
    leftRouter.Create(1);

    NodeContainer rightRouter;
    rightRouter.Create(1);

    NodeContainer receiverNode;
    receiverNode.Create(1);

    // Point-to-Point links
    PointToPointHelper p2pLink;
    p2pLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLink.SetChannelAttribute("Delay", StringValue("10ms"));

    // Left side links (senders to left router)
    NetDeviceContainer senderDevices[7];
    for (uint32_t i = 0; i < 7; ++i)
    {
        senderDevices[i] = p2pLink.Install(senders.Get(i), leftRouter.Get(0));
    }

    // Middle link (left router to right router)
    NetDeviceContainer middleLinkDevices = p2pLink.Install(leftRouter.Get(0), rightRouter.Get(0));

    // Right side link (right router to receiver)
    NetDeviceContainer receiverLinkDevices = p2pLink.Install(rightRouter.Get(0), receiverNode.Get(0));

    // Install Queue Discs
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueDiscType);

    // Set queue size to prevent bufferbloat in the bottleneck
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize",
                       QueueSizeValue(QueueSize("1000p")));

    // Install queue disc on middle link at left router
    QueueDiscContainer qDiscs;
    qDiscs = tch.Install(middleLinkDevices.Get(0)); // install on sender side device

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces[7];
    for (uint32_t i = 0; i < 7; ++i)
    {
        senderInterfaces[i] = address.Assign(senderDevices[i]);
        address.NewNetwork();
    }

    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer middleInterfaces = address.Assign(middleLinkDevices);

    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterface = address.Assign(receiverLinkDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // TCP applications
    uint16_t port = 50000;
    for (uint32_t i = 0; i < 7; ++i)
    {
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(receiverNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        // Setup OnOff application over TCP
        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(receiverInterface.GetAddress(1), port));
        client.SetConstantRate(DataRate("1Mbps"), 512);
        ApplicationContainer clientApp = client.Install(senders.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(9.0));

        port++;
    }

    // UDP application
    port = 60000;
    UdpServerHelper udpServer(port);
    ApplicationContainer udpSinkApp = udpServer.Install(receiverNode.Get(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(receiverInterface.GetAddress(1), port);
    udpClient.SetMaxBytes(1000000000);
    ApplicationContainer udpClientApp = udpClient.Install(senders.Get(0));
    udpClientApp.Start(Seconds(2.0));
    udpClientApp.Stop(Seconds(8.0));

    // Tracing congestion window for TCP flows
    AsciiTraceHelper asciiTraceHelper;
    std::ostringstream cwndStream;
    cwndStream << "cwnd-" << queueDiscType << ".tr";
    Ptr<OutputStreamWrapper> cwndOsw = asciiTraceHelper.CreateFileStream(cwndStream.str());

    for (uint32_t i = 0; i < 7; ++i)
    {
        Config::Connect("/NodeList/" + std::to_string(senders.Get(i)->GetId()) +
                        "/ApplicationList/0/$ns3::PacketSink/Rx",
                        MakeBoundCallback(&CwndTracer, cwndOsw));
    }

    // Tracing queue size
    std::ostringstream queueSizeStream;
    queueSizeStream << "queue-size-" << queueDiscType << ".tr";
    Ptr<OutputStreamWrapper> queueOsw = asciiTraceHelper.CreateFileStream(queueSizeStream.str());

    qDiscs.Get(0)->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&QueueSizeTracer, queueOsw));

    // Enable flow monitoring
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flowmon-" + queueDiscType + ".xml", true, true);

    Simulator::Destroy();
}

int
main(int argc, char* argv[])
{
    // Run experiment with COBALT and CoDel
    experiment("COBALT");
    experiment("CoDel");

    return 0;
}