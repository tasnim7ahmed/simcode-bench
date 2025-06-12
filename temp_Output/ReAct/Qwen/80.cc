#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TbfExample");

int main(int argc, char *argv[]) {
    double bucketSize = 1000; // bytes
    double peakRate = 512000; // bps
    double tokenRate = 256000; // bps
    double bucketTwoSize = 500; // bytes
    double simulationTime = 10.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("bucketSize", "Size of the first token bucket (bytes)", bucketSize);
    cmd.AddValue("peakRate", "Peak rate (bps)", peakRate);
    cmd.AddValue("tokenRate", "Token arrival rate (bps)", tokenRate);
    cmd.AddValue("bucketTwoSize", "Size of the second token bucket (bytes)", bucketTwoSize);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TrafficControlHelper tch;
    uint16_t handle = 1;
    uint16_t pfifoHandle = 2;
    ObjectFactory queueDisc("ns3::TbfQueueDisc",
                            "MaxSize", StringValue("100p"),
                            "Burst", UintegerValue(bucketSize),
                            "Mtu", UintegerValue(bucketTwoSize),
                            "Rate", DataRateValue(DataRate(tokenRate)),
                            "PeakRate", DataRateValue(DataRate(peakRate)));
    tch.SetRootQueueDisc(queueDisc);
    QueueDiscContainer qdiscs = tch.Install(devices.Get(0));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
    onoff.SetConstantRate(DataRate("1Mbps"), 500);
    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 9)));
    app = sink.Install(nodes.Get(1));
    app.Start(Seconds(0.0));
    app.Stop(Seconds(simulationTime));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tbf-example.tr");
    qdiscs.Get(0)->TraceConnectWithoutContext("TokensInFirstBucket", MakeBoundCallback(&AsciiTraceHelper::DefaultEnqueueTracer, stream));
    qdiscs.Get(0)->TraceConnectWithoutContext("TokensInSecondBucket", MakeBoundCallback(&AsciiTraceHelper::DefaultDequeueTracer, stream));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        std::cout << "Flow " << i->first << ": " << i->second.GetSource() << " -> " << i->second.GetDestination() << std::endl;
        std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}