#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    // Create p2p links: node0 <-> node1 <-> node2
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev0 = p2p.Install(n0n1);
    NetDeviceContainer dev1 = p2p.Install(n1n2);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(dev0);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(dev1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP server on node 2 (using PacketSinkApp)
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(i1i2.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Install TCP client on node 0 (BulkSendApp)
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("p2p-tcp-three-nodes");

    // Install FlowMonitor to record throughput and packet loss
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    uint32_t totalLostPackets = 0;

    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == i1i2.GetAddress(1) && t.destinationPort == port)
        {
            double duration = (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds());
            double throughput = duration > 0 ? (flow.second.rxBytes * 8.0 / duration / 1e6) : 0;
            totalThroughput += throughput;
            totalLostPackets += flow.second.lostPackets;
            std::cout << "Flow ID: " << flow.first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
            std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
            std::cout << "Packets Lost: " << flow.second.lostPackets << std::endl;
        }
    }

    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
    std::cout << "Total Packets Lost: " << totalLostPackets << std::endl;

    Simulator::Destroy();
    return 0;
}