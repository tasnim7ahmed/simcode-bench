#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

int main(int argc, char *argv[]) {
    // Ensure output directories exist
    system("mkdir -p results/cwndTraces");
    system("mkdir -p results/pcap");
    system("mkdir -p results/queueTraces");
    system("mkdir -p results/queueStats");

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Links: n0-n1 (10 Mbps, 1ms), n1-n2 (1 Mbps, 10ms), n2-n3 (10 Mbps, 1ms)
    PointToPointHelper p2p01;
    p2p01.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    p2p12.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p23.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    // Install links between nodes
    NetDeviceContainer dev01 = p2p01.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev12 = p2p12.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev23 = p2p23.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign(dev01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address.Assign(dev12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = address.Assign(dev23);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP flow from n0 to n3 using BulkSendApplication
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(if23.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer sourceApps = bulkSend.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2p01.EnablePcapAll("results/pcap/link01");
    p2p12.EnablePcapAll("results/pcap/link12");
    p2p23.EnablePcapAll("results/pcap/link23");

    // Tracing cwnd
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> cwndStream = asciiTraceHelper.CreateFileStream("results/cwndTraces/cwnd_trace.tr");

    Config::Connect("/NodeList/0/ApplicationList/0/$ns3::BulkSendApplication/Socket/TxStream", MakeBoundCallback(&TcpSocket::TraceCwnd, cwndStream));
    Config::Connect("/NodeList/3/ApplicationList/0/$ns3::PacketSink/Rx", MakeBoundCallback(&TcpSocket::TraceCwnd, cwndStream));

    // Queue size and drop tracing
    std::ofstream queueSizeFile("results/queue-size.dat");
    std::ofstream queueDropFile("results/queueTraces/drops.txt");

    // Monitor queue sizes on all devices
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        for (uint32_t j = 0; j < nodes.Get(i)->GetNDevices(); ++j) {
            std::ostringstream oss;
            oss << "/NodeList/" << i << "/DeviceList/" << j << "/$ns3::PointToPointNetDevice/Queue/BytesInQueue";
            Config::ConnectWithoutContext(oss.str(), MakeBoundCallback(&TcpSocket::TraceQueueSize, &queueSizeFile));
            oss.str("");
            oss << "/NodeList/" << i << "/DeviceList/" << j << "/$ns3::PointToPointNetDevice/Queue/Drop";
            Config::ConnectWithoutContext(oss.str(), MakeBoundCallback(&TcpSocket::TraceDrops, &queueDropFile));
        }
    }

    // Flow monitor for statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    monitor->SetAttribute("StartTime", TimeValue(Seconds(0)));
    monitor->SetAttribute("StopTime", TimeValue(Seconds(10)));

    // Write configuration details
    std::ofstream config("results/config.txt");
    config << "Link Configuration:\n";
    config << "n0 <-> n1: 10 Mbps, 1 ms delay\n";
    config << "n1 <-> n2: 1 Mbps, 10 ms delay\n";
    config << "n2 <-> n3: 10 Mbps, 1 ms delay\n";
    config << "Application: TCP BulkSend from n0 to n3\n";
    config << "Simulation Duration: 10 seconds\n";
    config.close();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output flow stats
    monitor->SerializeToXmlFile("results/queueStats/flow_stats.xml", false, false);

    Simulator::Destroy();
    return 0;
}