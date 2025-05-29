#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPointToPointSimulation");

static void
CalculateThroughput(Ptr<Socket> sock, uint64_t lastTotalRx, Ptr<Application> app, double interval, Ptr<OutputStreamWrapper> stream)
{
    if (!app->GetObject<Application>()->GetApplication())
        return;

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
    uint64_t currentTotalRx = sink->GetTotalRx();
    double throughput = (currentTotalRx - lastTotalRx) * 8.0 / (interval * 1e6); // Mbps
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << throughput << " Mbps\n";
    Simulator::Schedule(Seconds(interval), &CalculateThroughput, sock, currentTotalRx, app, interval, stream);
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    LogComponentEnable("TcpPointToPointSimulation", LOG_LEVEL_INFO);

    // Nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Point-to-Point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // Enable pcap and ASCII tracing
    p2p.EnablePcapAll("tcp-p2p");
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("tcp-p2p.tr"));

    // Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 8080;

    // TCP Server on Node A
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // TCP Client on Node B
    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
    clientHelper.SetAttribute("DataRate", StringValue("4Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1));

    // Throughput calculation
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("throughput.txt");
    *stream->GetStream() << "Time(s)\tThroughput(Mbps)\n";
    Simulator::Schedule(Seconds(1.1), &CalculateThroughput, nullptr, uint64_t(0), sink, 1.0, stream);

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if ((t.destinationAddress == interfaces.GetAddress(0) && t.destinationPort == port) ||
            (t.sourceAddress == interfaces.GetAddress(1) && t.sourcePort != 0))
        {
            NS_LOG_INFO("FlowId: " << flow.first
                         << ", Src: " << t.sourceAddress << ", Dst: " << t.destinationAddress
                         << ", TxPackets: " << flow.second.txPackets
                         << ", RxPackets: " << flow.second.rxPackets
                         << ", LostPackets: " << flow.second.lostPackets
                         << ", Throughput: " << (flow.second.rxBytes * 8.0 / (9.0 * 1e6)) << " Mbps");
        }
    }

    Simulator::Destroy();
    return 0;
}