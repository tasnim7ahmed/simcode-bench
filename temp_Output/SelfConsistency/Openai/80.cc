#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

// Global output streams for tracing
std::ofstream g_firstBucketTrace;
std::ofstream g_secondBucketTrace;

// Tracing functions
void
FirstBucketTrace(uint32_t tokens)
{
    g_firstBucketTrace << Simulator::Now().GetSeconds() << "\t" << tokens << std::endl;
}

void
SecondBucketTrace(uint32_t tokens)
{
    g_secondBucketTrace << Simulator::Now().GetSeconds() << "\t" << tokens << std::endl;
}

int
main(int argc, char *argv[])
{
    // Default TBF parameters
    uint32_t firstBucketSize = 1500 * 10; // bytes
    uint32_t secondBucketSize = 1500 * 2; // bytes
    std::string rate = "5Mbps";
    std::string peakRate = "10Mbps";

    // Traffic parameters
    std::string dataRate = "20Mbps";
    std::string delay = "2ms";

    // Parse command-line arguments
    CommandLine cmd;
    cmd.AddValue("firstBucketSize",   "First bucket size (bytes)", firstBucketSize);
    cmd.AddValue("secondBucketSize",  "Second bucket size (bytes)", secondBucketSize);
    cmd.AddValue("rate",              "Token arrival rate (e.g., '5Mbps')", rate);
    cmd.AddValue("peakRate",          "Peak rate (e.g., '10Mbps')", peakRate);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // --- Traffic Control, TBF Setup ---
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::TbfQueueDisc",
        "BurstSize", UintegerValue(firstBucketSize),
        "PeakBurstSize", UintegerValue(secondBucketSize),
        "Rate", DataRateValue(DataRate(rate)),
        "PeakRate", DataRateValue(DataRate(peakRate)));

    QueueDiscContainer qdiscs = tch.Install(devices.Get(0));

    // Open tracing files
    g_firstBucketTrace.open("first_bucket_trace.txt");
    g_secondBucketTrace.open("second_bucket_trace.txt");

    // Attach tracing for tokens in the TBF buckets
    Ptr<QueueDisc> tbfQdisc = qdiscs.Get(0);
    tbfQdisc->TraceConnectWithoutContext("Tokens", MakeCallback(&FirstBucketTrace));
    tbfQdisc->TraceConnectWithoutContext("PeakTokens", MakeCallback(&SecondBucketTrace));

    // --- Application Setup (OnOff) ---
    uint16_t port = 5000;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("DataRate", StringValue("8Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(9.0));

    // UDP Sink
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output TBF statistics
    Ptr<QueueDisc> q = qdiscs.Get(0);
    std::cout << "TBF Queue Statistics:" << std::endl;
    std::cout << "    Packets enqueued    : " << q->GetTotalEnqueuedPackets() << std::endl;
    std::cout << "    Packets dequeued    : " << q->GetTotalDequeuedPackets() << std::endl;
    std::cout << "    Packets dropped     : " << q->GetTotalDroppedPackets() << std::endl;
    std::cout << "    Bytes enqueued      : " << q->GetTotalEnqueuedBytes() << std::endl;
    std::cout << "    Bytes dequeued      : " << q->GetTotalDequeuedBytes() << std::endl;
    std::cout << "    Bytes dropped       : " << q->GetTotalDroppedBytes() << std::endl;

    if (g_firstBucketTrace.is_open())
        g_firstBucketTrace.close();
    if (g_secondBucketTrace.is_open())
        g_secondBucketTrace.close();

    Simulator::Destroy();
    return 0;
}