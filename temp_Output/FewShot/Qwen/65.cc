#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("PingApplication", LOG_LEVEL_INFO);

    // Set simulation stop time
    double stopTime = 10.0;
    bool pcapEnabled = true;

    // Create nodes (Client, Bottleneck Switch, Server)
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point links
    PointToPointHelper bottleNeckLink;
    bottleNeckLink.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    bottleNeckLink.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper clientLink;
    clientLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    clientLink.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper serverLink;
    serverLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    serverLink.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install devices
    NetDeviceContainer clientDevices = clientLink.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer serverDevices = serverLink.Install(nodes.Get(2), nodes.Get(1));
    NetDeviceContainer bottleneckDevices = bottleNeckLink.Install(nodes.Get(1), nodes.Get(2));

    // Traffic Control Setup on the bottleneck device
    TrafficControlHelper tchBottleneck;
    tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");

    tchBottleneck.Install(bottleneckDevices);

    // Internet stack with traffic control
    InternetStackHelper stack;
    stack.SetRoutingHelper(Ipv4GlobalRoutingHelper());
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);
    address.NewNetwork();
    Ipv4InterfaceContainer serverInterfaces = address.Assign(serverDevices);
    address.NewNetwork();
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ICMP Ping Application from Client to Server
    V4PingHelper ping(bottleneckInterfaces.GetAddress(1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(1.0));
    pingApp.Stop(Seconds(stopTime));

    // TCP BulkSend Application from Client to Server
    uint16_t tcpPort = 9000;
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", InetSocketAddress(bottleneckInterfaces.GetAddress(1), tcpPort));
    bulkSender.SetAttribute("MaxBytes", UintegerValue(1000000)); // Send 1MB of data
    ApplicationContainer senderApp = bulkSender.Install(nodes.Get(0));
    senderApp.Start(Seconds(2.0));
    senderApp.Stop(Seconds(stopTime));

    // Packet Sink on Server node for TCP
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer receiverApp = sink.Install(nodes.Get(2));
    receiverApp.Start(Seconds(1.0));
    receiverApp.Stop(Seconds(stopTime));

    // Queue Disc Tracing on the bottleneck device
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> qTraceStream = asciiTraceHelper.CreateFileStream("queue-disc-trace.txt");
    QueueDisc::EnableQueueDiscPacketsInQueueTrace(bottleneckDevices.Get(0), qTraceStream);
    QueueDisc::EnableQueueDiscDropTrace(bottleneckDevices.Get(0), qTraceStream);
    QueueDisc::EnableQueueDiscMarkTrace(bottleneckDevices.Get(0), qTraceStream);

    // RTT, CWND, Throughput tracing
    std::ofstream rttStream("rtt-trace.txt");
    std::ofstream cwndStream("cwnd-trace.txt");
    std::ofstream throughputStream("throughput-trace.txt");

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket/Tx", MakeBoundCallback(&ThroughputMonitor, &throughputStream));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket/CongestionWindow", MakeBoundCallback(&CwndChange, &cwndStream));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::BulkSendApplication/Socket/Rtt", MakeBoundCallback(&RttChange, &rttStream));

    // Flow Monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // PCAP tracing if enabled
    if (pcapEnabled) {
        bottleneckLink.EnablePcapAll("bottleneck-link-pcap");
        clientLink.EnablePcapAll("client-link-pcap");
        serverLink.EnablePcapAll("server-link-pcap");
    }

    // Schedule simulation stop
    Simulator::Stop(Seconds(stopTime));

    // Run the simulation
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / stopTime / 1000 / 1000 << " Mbps\n";
    }

    // Clean up
    Simulator::Destroy();

    return 0;
}

// Helper callbacks for tracing
void ThroughputMonitor(std::ofstream *os, Ptr<const Packet> packet) {
    *os << Simulator::Now().GetSeconds() << " " << packet->GetSize() << std::endl;
}

void CwndChange(std::ofstream *os, uint32_t oldCwnd, uint32_t newCwnd) {
    *os << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
}

void RttChange(std::ofstream *os, const Time oldRtt, const Time newRtt) {
    *os << Simulator::Now().GetSeconds() << " " << newRtt.GetMilliSeconds() << std::endl;
}