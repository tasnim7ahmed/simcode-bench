#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set default TCP protocol to TcpNewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Enable logging for BulkSendApplication and PacketSink
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create two nodes: client and server
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link with configurable data rate and delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP receiver (PacketSink) on server node (node 1)
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP bulk send application on client node (node 0)
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
    ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    // Enable pcap tracing for packet analysis
    pointToPoint.EnablePcapAll("tcp-bulk-transfer");

    // Set up FlowMonitor to track throughput and bytes transmitted
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // Run simulation
    Simulator::Run();

    // Print per-flow statistics
    flowMonitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        NS_LOG_INFO("Flow ID: " << it->first);
        NS_LOG_INFO("  Tx Bytes: " << it->second.txBytes);
        NS_LOG_INFO("  Rx Bytes: " << it->second.rxBytes);
        NS_LOG_INFO("  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps");
    }

    Simulator::Destroy();
    return 0;
}