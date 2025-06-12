#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void
CalculateThroughput(Ptr<FlowMonitor> flowMonitor)
{
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    double totalThroughput = 0.0;
    uint64_t totalLostPackets = 0;
    for (const auto& flow : stats)
    {
        double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
        totalThroughput += throughput;
        totalLostPackets += flow.second.lostPackets;
        std::cout << "Flow " << flow.first 
                  << " throughput: " << throughput << " Mbps, "
                  << "lost packets: " << flow.second.lostPackets << std::endl;
    }
    std::cout << "Total throughput: " << totalThroughput << " Mbps" << std::endl;
    std::cout << "Total lost packets: " << totalLostPackets << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable logging for Tcp
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: N0 (client), N1 (router), N2 (server)
    NodeContainer nodes;
    nodes.Create(3);

    // Create two point-to-point links: N0<->N1 and N1<->N2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer dev01, dev12;
    dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface01 = address.Assign(dev01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface12 = address.Assign(dev12);

    // Enable PCAP tracing
    p2p.EnablePcapAll("p2p-tcp-sim");

    // Set up default route for node 0 (client)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> staticRouting0 = ipv4RoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    staticRouting0->SetDefaultRoute(iface01.GetAddress(1), 1);

    // Set up static routing on node 2 (server)
    Ptr<Ipv4StaticRouting> staticRouting2 = ipv4RoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    staticRouting2->SetDefaultRoute(iface12.GetAddress(0), 1);

    // Setup TCP server (PacketSink) on node 2
    uint16_t port = 8080;
    Address sinkAddress (InetSocketAddress(iface12.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(20.0));

    // Setup TCP client (BulkSendApplication) on node 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // Send unlimited data
    ApplicationContainer clientApp = bulkSender.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Install FlowMonitor to collect stats
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    CalculateThroughput(monitor);

    Simulator::Destroy();
    return 0;
}