#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    CommandLine cmd;
    uint32_t bucketSize1 = 1000;
    uint32_t bucketSize2 = 2000;
    std::string tokenRate = "1Mbps";
    std::string peakRate = "10Mbps";
    std::string onTime = "ns3::ConstantRandomVariable[Constant=1]";
    std::string offTime = "ns3::ConstantRandomVariable[Constant=0]";
    std::string appDataRate = "5Mbps";
    bool enablePcap = false;

    cmd.AddValue("bucketSize1", "Size of the first bucket in bytes", bucketSize1);
    cmd.AddValue("bucketSize2", "Size of the second bucket in bytes", bucketSize2);
    cmd.AddValue("tokenRate", "Token arrival rate", tokenRate);
    cmd.AddValue("peakRate", "Peak rate for the TBF", peakRate);
    cmd.AddValue("onTime", "On Time", onTime);
    cmd.AddValue("offTime", "Off Time", offTime);
    cmd.AddValue("appDataRate", "Application Data Rate", appDataRate);
    cmd.AddValue("enablePcap", "Enable PCAP Tracing", enablePcap);

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
    tch.SetRootQueueDisc("ns3::TbfQueueDisc",
                         "MaxSize1", StringValue(std::to_string(bucketSize1) + "b"),
                         "MaxSize2", StringValue(std::to_string(bucketSize2) + "b"),
                         "TokenRate", StringValue(tokenRate),
                         "PeakRate", StringValue(peakRate));

    QueueDiscContainer qdiscs = tch.Install(devices.Get(0));

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(1), tid);
    InetSocketAddress local = InetSocketAddress(interfaces.GetAddress(1), 8080);
    recvSink->Bind(local);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 8080)));
    onoff.SetAttribute("OnTime", StringValue(onTime));
    onoff.SetAttribute("OffTime", StringValue(offTime));
    onoff.SetAttribute("DataRate", StringValue(appDataRate));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 8080));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    if (enablePcap) {
        pointToPoint.EnablePcapAll("tbf-example");
    }

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream1 = ascii.CreateFileStream("tbf-bucket1.txt");
    qdiscs.Get(0)->TraceConnectWithoutContext("Bucket1NumBytes",
                                              "tbf-example-bucket1",
                                              MakeBoundCallback(&QueueDisc::ReportQueueSize<uint32_t>, stream1));

    Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream("tbf-bucket2.txt");
    qdiscs.Get(0)->TraceConnectWithoutContext("Bucket2NumBytes",
                                              "tbf-example-bucket2",
                                              MakeBoundCallback(&QueueDisc::ReportQueueSize<uint32_t>, stream2));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    qdiscs.Get(0)->PrintStatistics(std::cout);

    Simulator::Destroy();
    return 0;
}