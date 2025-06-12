#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

static void
ThroughputMonitor(Ptr<Application> app, uint64_t lastTotalRx, Ptr<OutputStreamWrapper> stream)
{
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
    uint64_t curTotalRx = sink->GetTotalRx();
    double throughput = (curTotalRx - lastTotalRx) * 8.0 / 1e6; // Mbps
    *stream->GetStream() << Simulator::Now().GetSeconds() << "s\t" << throughput << " Mbps" << std::endl;
    Simulator::Schedule(Seconds(1.0), &ThroughputMonitor, app, curTotalRx, stream);
}

int main(int argc, char *argv[])
{
    // Enable logging for TCP applications
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2); // Node A (0): server, Node B (1): client

    // Set up point-to-point attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and channels
    NetDeviceContainer devices = p2p.Install(nodes);

    // Enable PCAP tracing
    p2p.EnablePcapAll("tcp-p2p");
    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("tcp-p2p.tr"));

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP server (PacketSink) on Node A (node 0)
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(12.0));

    // Install TCP client (BulkSendApplication) on Node B (node 1)
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // send unlimited data
    ApplicationContainer clientApp = bulkSender.Install(nodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(11.0));

    // Throughput monitor output stream
    AsciiTraceHelper throughputAscii;
    Ptr<OutputStreamWrapper> throughputStream = throughputAscii.CreateFileStream("tcp-throughput.txt");

    // Schedule throughput monitoring, start after server starts
    Simulator::Schedule(Seconds(1.1), &ThroughputMonitor, sinkApp.Get(0), uint64_t(0), throughputStream);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}