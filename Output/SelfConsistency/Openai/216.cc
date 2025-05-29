#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpP2PThreeNodeExample");

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpP2PThreeNodeExample", LOG_LEVEL_INFO);
    // Command line arguments if needed
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create node containers for each link
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

    // Set up point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Enable PCAP tracing
    p2p.EnablePcapAll("tcp-p2p-3node");

    // Install the internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP server on node 2
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(i1i2.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", 
                                      InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(12.0));

    // Install TCP client on node 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(11.0)));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));

    // Flow Monitor for performance statistics
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(13.0));
    Simulator::Run();

    // Gather statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    uint64_t totalLostPackets = 0;
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == i1i2.GetAddress(1) && t.destinationPort == port)
        {
            double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6; // Mbps
            totalThroughput += throughput;
            totalLostPackets += flow.second.lostPackets;

            std::cout << "Flow ID: " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
            std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
            std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
            std::cout << "  Throughput: " << throughput << " Mbps\n";
        }
    }
    std::cout << "Total Throughput: " << totalThroughput << " Mbps\n";
    std::cout << "Total Lost Packets: " << totalLostPackets << std::endl;

    Simulator::Destroy();
    return 0;
}