#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTcpSimulation");

int main(int argc, char *argv[])
{
    // Enable logging for this component
    LogComponentEnable("SimpleTcpSimulation", LOG_LEVEL_INFO);

    // Set the default TCP congestion control algorithm to NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Install devices on nodes
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install internet stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configure PacketSink application (server) on node 1
    uint16_t port = 9; // Standard echo port often used, can be any unused port
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1)); // Server on node 1
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0)); // Run for the entire simulation duration

    // Configure OnOff application (client) on node 0
    OnOffHelper onoff("ns3::TcpSocketFactory", Ipv4Address::GetZero()); // Dummy address for OnOff constructor
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    onoff.SetAttribute("DataRate", StringValue("5Mbps")); // Target sending rate from client
    // Set the remote address to the server's IP and port
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(1), port)));

    ApplicationContainer clientApps = onoff.Install(nodes.Get(0)); // Client on node 0
    clientApps.Start(Seconds(1.0)); // Start sending at 1 second
    clientApps.Stop(Seconds(9.0));  // Stop sending at 9 seconds (traffic generation for 8 seconds)

    // Install FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Flow Monitor statistics retrieval and output
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalThroughputMbps = 0.0;
    uint32_t totalBytesReceived = 0;
    double appDuration = 8.0; // Client application runs from 1s to 9s

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        // Check if this is the TCP flow from node 0 (source) to node 1 (destination)
        // Protocol 6 is TCP
        if (t.protocol == 6 &&
            t.sourceAddress == interfaces.GetAddress(0) &&
            t.destinationAddress == interfaces.GetAddress(1))
        {
            NS_LOG_INFO("  Flow ID: " << i->first);
            NS_LOG_INFO("  Source IP: " << t.sourceAddress << " Destination IP: " << t.destinationAddress);
            NS_LOG_INFO("  Source Port: " << t.sourcePort << " Destination Port: " << t.destinationPort);
            NS_LOG_INFO("  Tx Packets: " << i->second.txPackets);
            NS_LOG_INFO("  Rx Packets: " << i->second.rxPackets);
            NS_LOG_INFO("  Lost Packets: " << i->second.lostPackets);
            NS_LOG_INFO("  Bytes Received: " << i->second.rxBytes);

            // Calculate throughput in Mbps
            // (Total bytes * 8 bits/byte) / (duration in seconds * 10^6 bits/Mbps)
            double throughput = (i->second.rxBytes * 8.0) / (appDuration * 1e6); // in Mbps
            NS_LOG_INFO("  Throughput: " << throughput << " Mbps");
            totalThroughputMbps += throughput;
            totalBytesReceived += i->second.rxBytes;
        }
    }

    NS_LOG_INFO("Total Throughput for relevant flows: " << totalThroughputMbps << " Mbps");
    NS_LOG_INFO("Total Bytes Received for relevant flows: " << totalBytesReceived << " bytes");

    Simulator::Destroy();

    return 0;
}