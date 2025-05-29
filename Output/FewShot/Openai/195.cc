#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

static uint64_t totalBytesReceived = 0;
static Time lastTime = Seconds(0.0);

void
RxCallback(Ptr<const Packet> packet, const Address &address)
{
    totalBytesReceived += packet->GetSize();
}

void
ThroughputMonitor()
{
    double timeNow = Simulator::Now().GetSeconds();
    double throughput = (totalBytesReceived * 8.0) / (timeNow - lastTime.GetSeconds()) / 1e6; // Mbps
    std::cout << Simulator::Now().GetSeconds() << "s: "
              << "Total bytes received: " << totalBytesReceived
              << ", Throughput: " << throughput << " Mbps" << std::endl;
    lastTime = Simulator::Now();
    Simulator::Schedule(Seconds(1.0), &ThroughputMonitor);
}

int
main(int argc, char *argv[])
{
    std::string dataRate = "10Mbps";
    std::string delay = "5ms";
    CommandLine cmd;
    cmd.AddValue("dataRate", "Point-to-Point link data rate", dataRate);
    cmd.AddValue("delay", "Point-to-Point link delay", delay);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(12.0));

    BulkSendHelper source("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(12.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("tcp-bulk-send");

    // Log bytes received and throughput
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));
    lastTime = Seconds(1.0);
    Simulator::Schedule(Seconds(1.0), &ThroughputMonitor);

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}