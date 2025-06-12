#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

void experiment(std::string queueDiscType);

int main(int argc, char *argv[]) {
    // Run the experiment with COBALT and CoDel queue discs
    experiment("COBALT");
    experiment("CODEL");

    return 0;
}

void experiment(std::string queueDiscType) {
    // Set simulation time and file names
    double simTime = 10.0;
    std::string cwndFile = "tcp-cwnd-" + queueDiscType + ".txt";
    std::string qSizeFile = "queue-size-" + queueDiscType + ".txt";

    // Enable logging if needed
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 7 senders (n0-n6), 1 bottleneck router (r1/r2), 1 receiver (n7)
    NodeContainer senders;
    senders.Create(7);

    NodeContainer routers;
    routers.Create(2);  // Two routers for dumbbell topology

    NodeContainer receiver;
    receiver.Create(1);

    // Connect senders to left router
    NetDeviceContainer senderDevices[7];
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    for (int i = 0; i < 7; ++i) {
        senderDevices[i] = p2p.Install(senders.Get(i), routers.Get(0));
    }

    // Connect routers via bottleneck link
    p2p.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer routerDevices = p2p.Install(routers.Get(0), routers.Get(1));

    // Connect right router to receiver
    NetDeviceContainer receiverDevice = p2p.Install(routers.Get(1), receiver.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer senderInterfaces[7], routerInterfaces[2], receiverInterface;

    address.SetBase("10.0.0.0", "255.255.255.0");
    for (int i = 0; i < 7; ++i) {
        senderInterfaces[i] = address.Assign(senderDevices[i]);
        address.NewNetwork();
    }

    address.SetBase("10.0.8.0", "255.255.255.0");
    routerInterfaces[0] = address.Assign(routerDevices);
    address.NewNetwork();

    address.SetBase("10.0.9.0", "255.255.255.0");
    receiverInterface = address.Assign(receiverDevice);

    // Setup routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup traffic control for the bottleneck link
    TrafficControlHelper tch;
    if (queueDiscType == "COBALT") {
        tch.SetRootQueueDisc("ns3::CobaltQueueDisc");
    } else if (queueDiscType == "CODEL") {
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
    }

    // Apply queue disc only on the left router's outgoing interface to the right router
    tch.Install(routerDevices.Get(0));

    // Setup applications
    uint16_t port = 9;

    // TCP sink (receiver)
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = tcpSink.Install(receiver.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // UDP echo server
    UdpEchoServerHelper udpServer(port);
    ApplicationContainer udpSinkApp = udpServer.Install(receiver.Get(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    // Install TCP and UDP clients on senders
    for (int i = 0; i < 7; ++i) {
        // TCP client
        OnOffHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(receiverInterface.GetAddress(0), port));
        tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        tcpClient.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer tcpApp = tcpClient.Install(senders.Get(i));
        tcpApp.Start(Seconds(1.0));
        tcpApp.Stop(Seconds(simTime));

        // UDP client
        OnOffHelper udpClient("ns3::UdpSocketFactory", InetSocketAddress(receiverInterface.GetAddress(0), port + 1));
        udpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        udpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        udpClient.SetAttribute("DataRate", DataRateValue(DataRate("0.5Mbps")));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer udpApp = udpClient.Install(senders.Get(i));
        udpApp.Start(Seconds(1.0));
        udpApp.Stop(Seconds(simTime));
    }

    // Congestion window tracing for TCP
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream(cwndFile);
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeBoundCallback(&PacketSinkRxTrace, cwndStream));

    // Queue size tracing
    Ptr<OutputStreamWrapper> qSizeStream = asciiTraceHelper.CreateFileStream(qSizeFile);
    Config::Connect("/NodeList/1/$ns3::Node/DeviceList/0/$ns3::PointToPointNetDevice/ReceiveQueue/Drop", MakeBoundCallback(&QueueDropTrace, qSizeStream));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
}