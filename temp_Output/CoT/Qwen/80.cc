#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TbfExample");

int main(int argc, char *argv[])
{
    double simulationTime = 10.0;
    uint32_t bucketSize = 1500;
    uint32_t peakRate = 1000000; // bits/s
    uint32_t rate = 500000;      // bits/s
    uint32_t mtu = 1500;

    CommandLine cmd(__FILE__);
    cmd.AddValue("bucketSize", "First Bucket Size (bytes)", bucketSize);
    cmd.AddValue("peakRate", "Peak Rate (bits/s)", peakRate);
    cmd.AddValue("rate", "Token Rate (bits/s)", rate);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::TbfQueueDisc",
                         "MaxSize", StringValue("100p"),
                         "Burst", UintegerValue(bucketSize),
                         "Mtu", UintegerValue(mtu),
                         "Rate", DataRateValue(DataRate(rate)),
                         "PeakRate", DataRateValue(DataRate(peakRate)));
    QueueDiscContainer qdiscs = tch.Install(devices);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
    onoff.SetConstantRate(DataRate("1Mbps"), 500);
    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime - 2));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), 9)));
    app = sink.Install(nodes.Get(1));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(simulationTime));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tbf-example.tr");
    qdiscs.Get(0)->TraceConnectWithoutContext("TokensInFirstBucket", MakeBoundCallback(&AsciiTraceHelper::DefaultEnqueueSinkUinteger32, stream));
    qdiscs.Get(0)->TraceConnectWithoutContext("TokensInSecondBucket", MakeBoundCallback(&AsciiTraceHelper::DefaultEnqueueSinkUinteger32, stream));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    Ptr<TbfQueueDisc> tbf = DynamicCast<TbfQueueDisc>(qdiscs.Get(0));
    if (tbf)
    {
        std::cout << "TBF Statistics:" << std::endl;
        std::cout << "Packets dropped: " << tbf->GetStats().drop << std::endl;
        std::cout << "Packets enqueued: " << tbf->GetStats().enqueue << std::endl;
        std::cout << "Packets dequeued: " << tbf->GetStats().dequeue << std::endl;
    }

    Simulator::Destroy();
    return 0;
}