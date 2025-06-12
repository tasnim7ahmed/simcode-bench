#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-bbr.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpBbr", LOG_LEVEL_ALL);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4); // Sender, R1, R2, Receiver

    NodeContainer senderR1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer R1R2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer R2Receiver = NodeContainer(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    // Configure links
    PointToPointHelper senderR1Link;
    senderR1Link.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    senderR1Link.SetChannelAttribute("Delay", StringValue("5ms"));

    PointToPointHelper R1R2Link;
    R1R2Link.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    R1R2Link.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper R2ReceiverLink;
    R2ReceiverLink.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    R2ReceiverLink.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer senderR1Devices = senderR1Link.Install(senderR1);
    NetDeviceContainer R1R2Devices = R1R2Link.Install(R1R2);
    NetDeviceContainer R2ReceiverDevices = R2ReceiverLink.Install(R2Receiver);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderR1Interfaces = address.Assign(senderR1Devices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer R1R2Interfaces = address.Assign(R1R2Devices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer R2ReceiverInterfaces = address.Assign(R2ReceiverDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP BBR on the sender
    TcpBbr agentFactory;
    GlobalValue::Bind("Protocol", StringValue("ns3::TcpBbr"));

    // Configure BulkSend application (Sender)
    uint16_t port = 50000;
    BulkSendHelper source("ns3::TcpSocketFactory",
                           InetSocketAddress(R2ReceiverInterfaces.GetAddress(1), port));
    source.SetAttribute("SendSize", UintegerValue(1400));
    source.SetAttribute("MaxBytes", UintegerValue(0)); // Send data indefinitely

    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(100.0));

    // Configure PacketSink application (Receiver)
    PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(100.0));

    // Enable PCAP tracing
    senderR1Link.EnablePcapAll("bbr_simulation_senderR1");
    R1R2Link.EnablePcapAll("bbr_simulation_R1R2");
    R2ReceiverLink.EnablePcapAll("bbr_simulation_R2Receiver");

    // Congestion Window trace
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("bbr_congestion_window.txt");
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Tx/CongestionWindow",
                                 MakeBoundCallback(&TcpBbr::TraceCwnd, stream));

    // Throughput trace
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Queue size trace (bottleneck link)
    AsciiTraceHelper asciiTrace;
    Ptr<OutputStreamWrapper> streamQueue = asciiTrace.CreateFileStream("bbr_queue_size.txt");
    R1R2Link.EnableAsciiAll(streamQueue);

    Simulator::Schedule(Seconds(10.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
    Simulator::Schedule(Seconds(20.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
        Simulator::Schedule(Seconds(30.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
        Simulator::Schedule(Seconds(40.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
        Simulator::Schedule(Seconds(50.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
        Simulator::Schedule(Seconds(60.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
        Simulator::Schedule(Seconds(70.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
        Simulator::Schedule(Seconds(80.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });
        Simulator::Schedule(Seconds(90.0), [](){
        Config::SetGlobal ("TcpBbr::ProbeRttInterval", TimeValue (Seconds(10.0)));
        Config::SetGlobal ("TcpBbr::ProbeRttMinCwndSegments", UintegerValue(4));
    });

    Simulator::Stop(Seconds(100.0));

    Simulator::Run();

    monitor->SerializeToXmlFile("bbr_flowmon.xml", false, true);

    Simulator::Destroy();

    return 0;
}