#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferExample");

static uint64_t g_totalBytes = 0;

void
RxCallback(Ptr<const Packet> packet, const Address &)
{
    g_totalBytes += packet->GetSize();
}

void
ThroughputMonitor(Ptr<PacketSink> sink, double lastTime, uint64_t lastBytes)
{
    Time now = Simulator::Now();
    double curTime = now.GetSeconds();
    uint64_t curBytes = sink->GetTotalRx();

    double throughput = ((curBytes - lastBytes) * 8.0) / (curTime - lastTime) / 1e6; // Mbps

    std::cout << "Time: " << curTime << "s, Bytes received: " << curBytes
              << ", Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Schedule(Seconds(1.0), &ThroughputMonitor, sink, curTime, curBytes);
}

int
main(int argc, char *argv[])
{
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";
    uint32_t maxBytes = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Data rate of the point-to-point link", dataRate);
    cmd.AddValue("delay", "Propagation delay of the point-to-point link", delay);
    cmd.AddValue("maxBytes", "Total number of bytes for BulkSendApplication (0 means infinite)", maxBytes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 5000;

    // Install PacketSink (TCP server) on node 1
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    // Install BulkSendApplication (TCP client) on node 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory",
                              InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer senderApp = bulkSender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(20.0));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp_bulk_transfer");

    // Schedule throughput monitoring
    Simulator::Schedule(Seconds(1.1), &ThroughputMonitor, sink, 1.0, 0);

    Simulator::Stop(Seconds(20.1));
    Simulator::Run();

    double simTime = 20.0 - 1.0; // duration in seconds (app runs from 1.0s to 20.0s)
    double throughput = (g_totalBytes * 8.0) / (simTime * 1e6); // Mbps
    std::cout << "=== Simulation Summary ===" << std::endl;
    std::cout << "Total Bytes Received: " << g_totalBytes << std::endl;
    std::cout << "Average Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}