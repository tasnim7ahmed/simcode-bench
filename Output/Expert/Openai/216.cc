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
    nodes.Create(3); // Node 0: client, Node 1: router, Node 2: server

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link between node 0 and node 1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Link between node 1 and node 2
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Install Internet stack
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

    // Install TCP server (PacketSink) on node 2
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(i1i2.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP client (BulkSend) on node 0
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited bytes
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing on all point-to-point devices
    p2p.EnablePcapAll("p2p-three-nodes");

    // Install FlowMonitor for statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Retrieve and output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if ((t.sourceAddress == i0i1.GetAddress(0) && t.destinationAddress == i1i2.GetAddress(1)) ||
            (t.sourceAddress == i1i2.GetAddress(1) && t.destinationAddress == i0i1.GetAddress(0)))
        {
            std::cout << "Flow ID: " << flow.first << " ["
                      << t.sourceAddress << " --> " << t.destinationAddress << "]\n";
            std::cout << "  Tx Bytes: " << flow.second.txBytes << "\n";
            std::cout << "  Rx Bytes: " << flow.second.rxBytes << "\n";
            std::cout << "  Throughput: "
                      << (flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds())/1e6)
                      << " Mbps\n";
            std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        }
    }

    Simulator::Destroy();
    return 0;
}