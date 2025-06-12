#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set default values for logging and output directories
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_ALL);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between consecutive nodes
    PointToPointHelper p2p_n0n1;
    p2p_n0n1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_n0n1.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_n2n3.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install devices on each link
    NetDeviceContainer d_n0n1 = p2p_n0n1.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d_n1n2 = p2p_n1n2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d_n2n3 = p2p_n2n3.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0n1 = address.Assign(d_n0n1);

    address.NewNetwork();
    Ipv4InterfaceContainer i_n1n2 = address.Assign(d_n1n2);

    address.NewNetwork();
    Ipv4InterfaceContainer i_n2n3 = address.Assign(d_n2n3);

    // Set up BulkSend application from n0 to n3
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(i_n2n3.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
    ApplicationContainer sourceApp = bulkSendHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p_n0n1.EnablePcapAll("results/pcap/n0n1");
    p2p_n1n2.EnablePcapAll("results/pcap/n1n2");
    p2p_n2n3.EnablePcapAll("results/pcap/n2n3");

    // Setup tracing of congestion window
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("results/cwndTraces/cwnd.tr");

    Config::Connect("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeBoundCallback(&CwndChange, cwndStream));

    // Queue size monitoring (on n1 -> n2 link)
    Ptr<PointToPointNetDevice> dev_n1n2 = DynamicCast<PointToPointNetDevice>(d_n1n2.Get(0));
    if (dev_n1n2) {
        dev_n1n2->GetQueue()->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueEnqueueTrace));
        dev_n1n2->GetQueue()->TraceConnectWithoutContext("Dequeue", MakeCallback(&QueueDequeueTrace));
        dev_n1n2->GetQueue()->TraceConnectWithoutContext("Drop", MakeCallback(&QueueDropTrace));
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Create output directories if not exist
    system("mkdir -p results results/cwndTraces results/queue-size results/pcap results/queueTraces results/queueStats");

    // Write configuration info
    std::ofstream configOut("results/config.txt");
    configOut << "Topology: Series of 4 nodes\n";
    configOut << "Link n0-n1: 10 Mbps, 1 ms delay\n";
    configOut << "Link n1-n2: 1 Mbps, 10 ms delay\n";
    configOut << "Link n2-n3: 10 Mbps, 1 ms delay\n";
    configOut << "Traffic: TCP BulkSend from n0 to n3\n";
    configOut << "Simulation Time: 10 seconds\n";
    configOut.close();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        std::cout << "Flow ID: " << iter->first << " Info: " << iter->second << std::endl;
    }

    Simulator::Destroy();
    return 0;
}