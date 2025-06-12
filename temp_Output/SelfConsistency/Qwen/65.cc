#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrafficControlSimulation");

void
EnqueueTrace(Ptr<QueueDisc> qd, Ptr<const Packet> p)
{
    std::cout << Simulator::Now().GetSeconds() << " Enqueue: Queue size = " << qd->GetCurrentSize() << std::endl;
}

void
DropTrace(Ptr<QueueDisc> qd, Ptr<const Packet> p)
{
    std::cout << Simulator::Now().GetSeconds() << " Drop: Queue size = " << qd->GetCurrentSize() << std::endl;
}

void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

int
main(int argc, char* argv[])
{
    bool enableDctcp = true;
    bool usePcap = true;

    // Node creation
    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer bottleneckNodes;
    bottleneckNodes.Add(nodes.Get(0));
    bottleneckNodes.Add(nodes.Get(1));

    // Point-to-point link setup
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = p2p.Install(bottleneckNodes);

    // Traffic control configuration
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");

    InternetStackHelper internet;
    if (enableDctcp)
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpDctcp::GetTypeId()));
    }
    internet.SetTosReader(true);
    internet.SetScheduler("ns3::RandomWalk");
    internet.InstallAll();

    // IPv4 address assignment
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications
    uint16_t port = 9;

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(11.0));

    PingHelper ping(interfaces.GetAddress(1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1)));
    ping.SetAttribute("Size", UintegerValue(512));
    ApplicationContainer pingApps = ping.Install(nodes.Get(0));
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(10.0));

    // Tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> qTraceStream = asciiTraceHelper.CreateFileStream("queue-trace.log");
    Ptr<QueueDisc> qd = TrafficControlHelper::GetRootQueueDiscOnDevice(devices.Get(1));
    qd->TraceConnectWithoutContext("Enqueue", MakeCallback(&EnqueueTrace));
    qd->TraceConnectWithoutContext("Drop", MakeCallback(&DropTrace));

    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("cwnd-trace.log");
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                  MakeBoundCallback(&CwndChange, cwndStream));

    if (usePcap)
    {
        p2p.EnablePcapAll("traffic-control-sim");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flow-monitor.xml", false, false);

    Simulator::Destroy();

    return 0;
}