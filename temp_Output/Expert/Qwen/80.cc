#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TbfExample");

int main(int argc, char *argv[])
{
    double bucketSize = 1000;         // First bucket size in bytes
    double peakBucketSize = 500;      // Second bucket size in bytes
    double rate = 1024 * 1024 * 8;    // Rate in bps (default: 1 Mbps)
    double peakRate = 2048 * 1024 * 8; // Peak rate in bps (default: 2 Mbps)
    double simulationTime = 10.0;     // Simulation time in seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("bucketSize", "Size of the token bucket filter first bucket in bytes", bucketSize);
    cmd.AddValue("peakBucketSize", "Size of the token bucket filter second bucket in bytes", peakBucketSize);
    cmd.AddValue("rate", "Token arrival rate in bits per second", rate);
    cmd.AddValue("peakRate", "Peak token rate in bits per second", peakRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TrafficControlHelper tch;
    uint16_t handle = 1;
    uint16_t qdiscHandle = tch.SetRootQueueDisc("ns3::TbfQueueDisc",
                                               "MaxSize", StringValue("1000p"),
                                               "Burst", DoubleValue(bucketSize),
                                               "PeakBurst", DoubleValue(peakBucketSize),
                                               "Rate", DataRateValue(DataRate(rate)),
                                               "PeakRate", DataRateValue(DataRate(peakRate)));
    tch.Install(devices);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
    onoff.SetConstantRate(DataRate(rate));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 9)));
    app = sink.Install(nodes.Get(1));
    app.Start(Seconds(0.0));
    app.Stop(Seconds(simulationTime));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tbf-example.tr");

    QueueDiscContainer qdiscs = tch.GetQueueDiscs();
    for (uint32_t i = 0; i < qdiscs.GetN(); ++i)
    {
        Config::ConnectWithoutContext("/NodeList/0/$ns3::TrafficControlLayer/RootQueueDiscList/" + std::to_string(i) + "/TokensInFirstBucket",
                                      MakeBoundCallback(&AsciiTraceHelper::DefaultDoubleSink, stream));
        Config::ConnectWithoutContext("/NodeList/0/$ns3::TrafficControlLayer/RootQueueDiscList/" + std::to_string(i) + "/TokensInSecondBucket",
                                      MakeBoundCallback(&AsciiTraceHelper::DefaultDoubleSink, stream));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        NS_LOG_UNCOND("Flow ID: " << iter->first << " "
                                  << "Tx Packets: " << iter->second.txPackets << " "
                                  << "Rx Packets: " << iter->second.rxPackets << " "
                                  << "Lost Packets: " << iter->second.lostPackets << " "
                                  << "Throughput: " << iter->second.rxBytes * 8.0 / simulationTime / 1000000 << " Mbps");
    }

    Simulator::Destroy();
    return 0;
}