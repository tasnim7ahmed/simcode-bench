#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc-module.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

using namespace ns3;

std::ofstream queueDropStream;
std::ofstream queueSizeStream;
std::ofstream queueStatsStream;
Ptr<QueueDisc> queueDisc12;

void
CwndChangeCallback(std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
    static std::ofstream cwndFile("results/cwndTraces", std::ios::app);
    cwndFile << Simulator::Now().GetSeconds() << '\t' << newCwnd << std::endl;
}

void
QueueDropCallback(std::string context, Ptr<const QueueDiscItem> item)
{
    queueDropStream << Simulator::Now().GetSeconds() << "\tDrop\n";
}

void
QueueSizeTrace()
{
    uint32_t qSize = queueDisc12 ? queueDisc12->GetCurrentSize().GetValue() : 0;
    queueSizeStream << Simulator::Now().GetSeconds() << "\t" << qSize << std::endl;
    Simulator::Schedule(MilliSeconds(1), &QueueSizeTrace);
}

void
WriteQueueStats()
{
    if (queueDisc12)
    {
        queueStatsStream << "TotalPacketsReceived: " << queueDisc12->GetStats().nPacketsReceived << std::endl;
        queueStatsStream << "TotalPacketsDropped: " << queueDisc12->GetStats().nPacketsDropped << std::endl;
        queueStatsStream << "BytesReceived: " << queueDisc12->GetStats().nBytesReceived << std::endl;
        queueStatsStream << "BytesDropped: " << queueDisc12->GetStats().nBytesDropped << std::endl;
    }
}

int main(int argc, char *argv[])
{
    // Directory setup
    system("mkdir -p results");
    system("mkdir -p results/pcap");

    // Open output files
    queueDropStream.open("results/queueTraces", std::ios::out);
    queueSizeStream.open("results/queue-size.dat", std::ios::out);
    queueStatsStream.open("results/queueStats.txt", std::ios::out);

    // Write configuration
    std::ofstream config("results/config.txt");
    config << "Topology: n0--(10Mbps,1ms)--n1--(1Mbps,10ms)--n2--(10Mbps,1ms)--n3\n";
    config << "TCP BulkSendApplication from n0 to n3\n";
    config << "n0-n1: 10Mbps, 1ms\n";
    config << "n1-n2: 1Mbps, 10ms\n";
    config << "n2-n3: 10Mbps, 1ms\n";
    config << "Queue and congestion window statistics stored under results/\n";
    config.close();

    // Enable Tcp logging
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // n0-n1
    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("1ms"));

    // n1-n2
    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("10ms"));
    p2p2.SetQueue("ns3::DropTailQueue", "MaxSize", QueueSizeValue(QueueSize("100p")));

    // n2-n3
    PointToPointHelper p2p3;
    p2p3.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p3.SetChannelAttribute("Delay", StringValue("1ms"));

    // Devices
    NetDeviceContainer d01 = p2p1.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p3.Install(nodes.Get(2), nodes.Get(3));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = ipv4.Assign(d01);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = ipv4.Assign(d12);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

    // Install queue disc on n1's NetDevice facing n2
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", QueueSizeValue(QueueSize("100p")));
    QueueDiscContainer qdiscs = tch.Install(d12);
    queueDisc12 = qdiscs.Get(0);

    // Route
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // BulkSendApplication from n0 to n3
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i23.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.5));
    sourceApp.Stop(Seconds(20.0));

    // Trace congestion window on n0's TCP socket
    Config::Connect(
        "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
        MakeCallback(&CwndChangeCallback));

    // Trace queue disc drops
    queueDisc12->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDropCallback));

    // Start periodic queue size trace
    Simulator::ScheduleNow(&QueueSizeTrace);

    // Enable PCAP
    p2p1.EnablePcapAll("results/pcap/n0-n1", false);
    p2p2.EnablePcapAll("results/pcap/n1-n2", false);
    p2p3.EnablePcapAll("results/pcap/n2-n3", false);

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Write out queue statistics
    WriteQueueStats();

    queueDropStream.close();
    queueSizeStream.close();
    queueStatsStream.close();

    Simulator::Destroy();
    return 0;
}