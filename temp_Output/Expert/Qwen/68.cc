#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[])
{
    std::string queueDiscType = "PfifoFast";
    uint32_t bottleneckBandwidthKbps = 1000;
    uint32_t bottleneckDelayMs = 10;
    bool enableBql = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("queueDiscType", "Queue disc type: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueDiscType);
    cmd.AddValue("bottleneckBandwidthKbps", "Bottleneck bandwidth in Kbps", bottleneckBandwidthKbps);
    cmd.AddValue("bottleneckDelayMs", "Bottleneck delay in ms", bottleneckDelayMs);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    p2pAccess.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.1)));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(bottleneckBandwidthKbps) + "kbps")));
    p2pBottleneck.SetChannelAttribute("Delay", TimeValue(MilliSeconds(bottleneckDelayMs)));

    NetDeviceContainer devAccess1, devAccess2, devBottleneck;
    devAccess1 = p2pAccess.Install(nodes.Get(0), nodes.Get(1));
    devBottleneck = p2pBottleneck.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAccess1 = address.Assign(devAccess1);
    address.NewNetwork();
    Ipv4InterfaceContainer ifBottleneck = address.Assign(devBottleneck);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    TrafficControlHelper tchBottleneck;
    if (queueDiscType == "PfifoFast")
    {
        tchBottleneck.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    }
    else if (queueDiscType == "ARED")
    {
        tchBottleneck.SetRootQueueDisc("ns3::AredQueueDisc");
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
        NS_ABORT_MSG("Unknown queue disc type: " << queueDiscType);
    }

    if (enableBql)
    {
        tchBottleneck.SetQueueLimits("ns3::DynamicQueueLimits");
    }

    tchBottleneck.Install(devBottleneck);

    uint16_t port = 5000;
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(ifBottleneck.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSource = source.Install(nodes.Get(0));
    appSource.Start(Seconds(1.0));
    appSource.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appSink = sink.Install(nodes.Get(2));
    appSink.Start(Seconds(0.0));
    appSink.Stop(Seconds(11.0));

    BulkSendHelper sourceReverse("ns3::TcpSocketFactory", InetSocketAddress(ifAccess1.GetAddress(1), port + 1));
    sourceReverse.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSourceReverse = sourceReverse.Install(nodes.Get(2));
    appSourceReverse.Start(Seconds(1.0));
    appSourceReverse.Stop(Seconds(10.0));

    PacketSinkHelper sinkReverse("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer appSinkReverse = sinkReverse.Install(nodes.Get(0));
    appSinkReverse.Start(Seconds(0.0));
    appSinkReverse.Stop(Seconds(11.0));

    V4PingHelper ping(nodes.Get(2)->GetObject<Ipv4>()->GetAddress(2, 0).GetLocal());
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(512));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(2.0));
    pingApp.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> qTrace = asciiTraceHelper.CreateFileStream("queue-disc-trace.txt");
    tchBottleneck.EnableQueueTrace(qTrace);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    monitor->SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("flow-monitor-output.xml", true, true);

    Simulator::Destroy();
    return 0;
}