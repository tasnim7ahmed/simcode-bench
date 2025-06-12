#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install PacketSink (TCP server) on node 1
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install BulkSendApplication (TCP client) on node 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited send
    ApplicationContainer senderApp = bulkSender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("p2p-tcp-bulk-send");

    // Throughput measurement: schedule periodic throughput reporting
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    double lastTotalRx = 0;

    void (*throughputPrinter)(Ptr<PacketSink>, double*) = 
        [](Ptr<PacketSink> sinkApp, double* lastRx){
            double cur = sinkApp->GetTotalRx();
            double diff = (cur - *lastRx) * 8.0 / 1e6; // Mbps
            *lastRx = cur;
            std::cout << "Time: " << Simulator::Now().GetSeconds()
                      << "s  Throughput: " << diff << " Mbps" << std::endl;
            if (Simulator::Now().GetSeconds() < 10.0)
                Simulator::Schedule(Seconds(1.0), throughputPrinter, sinkApp, lastRx);
        };

    Simulator::Schedule(Seconds(1.1), throughputPrinter, sink, &lastTotalRx);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}