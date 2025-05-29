#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPointToPointThroughput");

uint64_t g_bytesReceived = 0;

void
RxCallback(Ptr<const Packet> packet, const Address &address)
{
    g_bytesReceived += packet->GetSize();
}

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp-point-to-point");
    // Enable ascii tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-point-to-point.tr"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    // TCP Server on Node A (nodes.Get(0))
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Attach RX Callback for throughput measurement
    Ptr<Application> app = serverApp.Get(0);
    Ptr<PacketSink> pktSink = DynamicCast<PacketSink>(app);
    pktSink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    // TCP Client on Node B (nodes.Get(1)), generates CBR-like traffic
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(0), port)));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("4Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    double throughput = (g_bytesReceived * 8.0) / (8.0); // in bits/sec over 8 seconds
    std::cout << "Total Bytes Received: " << g_bytesReceived << std::endl;
    std::cout << "Average Throughput: " << throughput / 1e6 << " Mbps" << std::endl;

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.destinationPort == port) {
            std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
            std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
            std::cout << "  Throughput: "
                      << (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds())) / 1e6
                      << " Mbps\n";
        }
    }

    Simulator::Destroy();
    return 0;
}