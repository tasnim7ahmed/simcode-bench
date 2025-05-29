#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

void
TokenLevelTrace(std::string context, double oldValue, double newValue)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context << " " << newValue << std::endl;
}

int main(int argc, char *argv[])
{
    double firstBucketSize = 15000;      // bytes
    double secondBucketSize = 20000;     // bytes
    std::string tokenRate = "8000bps";   // token arrival rate
    std::string peakRate = "16000bps";   // peak sending rate

    CommandLine cmd;
    cmd.AddValue("firstBucketSize", "Size of the first bucket (bytes)", firstBucketSize);
    cmd.AddValue("secondBucketSize", "Size of the second bucket (bytes)", secondBucketSize);
    cmd.AddValue("tokenRate", "Token arrival rate (e.g. 8000bps)", tokenRate);
    cmd.AddValue("peakRate", "Peak rate (e.g. 16000bps)", peakRate);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::TbfQueueDisc",
       "BurstSize", DoubleValue(firstBucketSize),
       "PeakBurstSize", DoubleValue(secondBucketSize),
       "Rate", DataRateValue(DataRate(tokenRate)),
       "PeakRate", DataRateValue(DataRate(peakRate)));

    QueueDiscContainer qdiscs = tch.Install(devices.Get(0));

    Ptr<QueueDisc> tbf = qdiscs.Get(0);

    Simulator::Schedule(Seconds(0.1), [&] {
        Ptr<Object> obj = tbf;
        if (obj->GetInstanceTypeId() == TypeId::LookupByName("ns3::TbfQueueDisc"))
        {
            Ptr<TbfQueueDisc> tbfPtr = DynamicCast<TbfQueueDisc>(obj);
            if (tbfPtr)
            {
                tbfPtr->TraceConnectWithoutContext("TokenBucketTokens",
                    MakeCallback(&TokenLevelTrace));
                tbfPtr->TraceConnectWithoutContext("PeakBucketTokens",
                    MakeCallback(&TokenLevelTrace));
            }
        }
    });

    uint16_t port = 50000;
    OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute ("DataRate", StringValue("10Mbps"));
    onoff.SetAttribute ("PacketSize", UintegerValue(1024));
    onoff.SetAttribute ("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(11.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    Ptr<TbfQueueDisc> tbfPtr = DynamicCast<TbfQueueDisc>(tbf);
    if (tbfPtr)
    {
        std::cout << "TBF statistics at the end of simulation:" << std::endl;
        std::cout << "  PacketsEnqueued: " << tbfPtr->GetStats().nPacketsEnqueued << std::endl;
        std::cout << "  PacketsDequeued: " << tbfPtr->GetStats().nPacketsDequeued << std::endl;
        std::cout << "  PacketsDropped:  " << tbfPtr->GetStats().nPacketsDropped << std::endl;
        std::cout << "  BytesEnqueued:   " << tbfPtr->GetStats().nBytesEnqueued << std::endl;
        std::cout << "  BytesDequeued:   " << tbfPtr->GetStats().nBytesDequeued << std::endl;
        std::cout << "  BytesDropped:    " << tbfPtr->GetStats().nBytesDropped << std::endl;
    }

    Simulator::Destroy();
    return 0;
}