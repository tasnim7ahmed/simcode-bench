#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[])
{
    std::string queueDiscType = "PfifoFast";
    uint64_t bandwidth = 1000000; // 1 Mbps default
    Time delay = MilliSeconds(10);
    bool enableBql = false;
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("queueDiscType", "Queue disc type: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueDiscType);
    cmd.AddValue("bandwidth", "Bottleneck link bandwidth in bps", bandwidth);
    cmd.AddValue("delay", "Bottleneck link delay", delay);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    p2pAccess.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.1)));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bandwidth)));
    p2pBottleneck.SetChannelAttribute("Delay", TimeValue(delay));

    NetDeviceContainer devAccess1 = p2pAccess.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBottleneck = p2pBottleneck.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAccess1 = address.Assign(devAccess1);

    address.NewNetwork();
    Ipv4InterfaceContainer ifBottleneck = address.Assign(devBottleneck);

    TrafficControlHelper tchBottleneck;
    if (queueDiscType == "ARED")
    {
        tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc", "MaxSize", StringValue("1000p"));
    }
    else if (queueDiscType == "CoDel")
    {
        tchBottleneck.SetRootQueueDisc("ns3::CoDelQueueDisc");
    }
    else if (queueDiscType == "FqCoDel")
    {
        tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }
    else if (queueDiscType == "PIE")
    {
        tchBottleneck.SetRootQueueDisc("ns3::PieQueueDisc");
    }
    else
    {
        tchBottleneck.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    }

    if (enableBql)
    {
        tchBottleneck.SetQueueLimits("ns3::DynamicQueueLimits");
    }

    tchBottleneck.Install(devBottleneck.Get(0));
    tchBottleneck.Install(devBottleneck.Get(1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(ifBottleneck.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSource = source.Install(nodes.Get(0));
    appSource.Start(Seconds(1.0));
    appSource.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appSink = sink.Install(nodes.Get(2));
    appSink.Start(Seconds(0.0));
    appSink.Stop(Seconds(simulationTime));

    BulkSendHelper sourceReverse("ns3::TcpSocketFactory", InetSocketAddress(ifAccess1.GetAddress(0), port + 1));
    sourceReverse.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSourceReverse = sourceReverse.Install(nodes.Get(2));
    appSourceReverse.Start(Seconds(1.0));
    appSourceReverse.Stop(Seconds(simulationTime));

    PacketSinkHelper sinkReverse("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer appSinkReverse = sinkReverse.Install(nodes.Get(0));
    appSinkReverse.Start(Seconds(0.0));
    appSinkReverse.Stop(Seconds(simulationTime));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(ifBottleneck.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream("queue-limits.tr");
    tchBottleneck.EnableQueueLimitsLog(queueStream);

    Simulator::ScheduleNow(&TrafficControlLayer::SetTraceCallback, &tchBottleneck, MakeBoundCallback(&Ns3TcpQueueLog::EnqueueDropCb, queueStream));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    monitor->SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    monitor->SetAttribute("DelayHistogram", BooleanValue(true));
    monitor->SetAttribute("JitterHistogram", BooleanValue(true));
    monitor->SetAttribute("PacketSizeHistogram", BooleanValue(true));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}