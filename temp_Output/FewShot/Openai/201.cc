#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 5 nodes: 0=central, 1-4=peripheral
    NodeContainer nodes;
    nodes.Create(5);

    // Containers for each point-to-point leg
    std::vector<NodeContainer> p2pLegs(4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        p2pLegs[i] = NodeContainer(nodes.Get(0), nodes.Get(i + 1));
    }

    // Set up PointToPoint attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install NetDevices
    std::vector<NetDeviceContainer> devices(4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(p2pLegs[i]);
    }

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    std::vector<Ipv4InterfaceContainer> interfaces(4);
    char base[13];
    for (uint32_t i = 0; i < 4; ++i)
    {
        sprintf(base, "10.1.%d.0", i + 1);
        Ipv4AddressHelper address;
        address.SetBase(base, "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Install UDP Echo Server on central node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP Echo Clients on peripheral nodes
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(0), port); // destination: central node
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(echoClient.Install(nodes.Get(i + 1)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing on each point-to-point device
    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream oss;
        oss << "star-p2p-" << i;
        p2p.EnablePcapAll(oss.str());
    }

    // FlowMonitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Gather FlowMonitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalSentPackets = 0;
    double totalReceivedPackets = 0;
    double totalLostPackets = 0;
    double totalDelay = 0;
    double totalThroughput = 0;
    uint32_t nFlows = 0;

    for (const auto& flowPair : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowPair.first);
        std::cout << "Flow " << flowPair.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flowPair.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flowPair.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flowPair.second.lostPackets << "\n";
        std::cout << "  Delay Sum: " << flowPair.second.delaySum.GetSeconds() << " s\n";
        double throughput = 0;
        if (flowPair.second.rxPackets > 0 && flowPair.second.timeLastRxPacket.GetSeconds() > 0.0)
        {
            throughput = flowPair.second.rxBytes * 8.0 / (flowPair.second.timeLastRxPacket.GetSeconds() - flowPair.second.timeFirstTxPacket.GetSeconds()) / 1e6; // Mbps
            std::cout << "  Throughput: " << throughput << " Mbps\n";
        }
        else
        {
            std::cout << "  Throughput: 0 Mbps\n";
        }
        double pdr = (flowPair.second.txPackets > 0) ? double(flowPair.second.rxPackets) / flowPair.second.txPackets : 0;
        std::cout << "  Packet Delivery Ratio: " << pdr * 100 << " %\n";
        if (flowPair.second.rxPackets > 0)
        {
            totalDelay += flowPair.second.delaySum.GetSeconds() / flowPair.second.rxPackets;
            totalThroughput += throughput;
            nFlows++;
        }
        totalSentPackets += flowPair.second.txPackets;
        totalReceivedPackets += flowPair.second.rxPackets;
        totalLostPackets += flowPair.second.lostPackets;
    }

    // Log averages
    if (nFlows > 0)
    {
        double avgPdr = (totalSentPackets > 0) ? totalReceivedPackets / totalSentPackets : 0;
        std::cout << "\n===== Aggregate Metrics =====\n";
        std::cout << "Total Sent Packets: " << totalSentPackets << "\n";
        std::cout << "Total Received Packets: " << totalReceivedPackets << "\n";
        std::cout << "Total Lost Packets: " << totalLostPackets << "\n";
        std::cout << "Average Packet Delivery Ratio: " << avgPdr * 100 << " %\n";
        std::cout << "Average End-to-End Delay: " << (totalDelay / nFlows) << " s\n";
        std::cout << "Average Throughput: " << (totalThroughput / nFlows) << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}