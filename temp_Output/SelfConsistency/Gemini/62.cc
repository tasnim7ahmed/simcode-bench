#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("TcpPacingSimulation", LOG_LEVEL_INFO);

    // Configure default values
    bool enablePacing = true;
    double simulationTime = 10.0;
    uint32_t initialCwnd = 10; // segments
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";
    std::string bottleneckDataRate = "1Mbps";
    std::string bottleneckDelay = "10ms";

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue("enablePacing", "Enable TCP pacing (true or false)", enablePacing);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("initialCwnd", "Initial congestion window (segments)", initialCwnd);
    cmd.AddValue("dataRate", "Data rate of non-bottleneck links", dataRate);
    cmd.AddValue("delay", "Delay of non-bottleneck links", delay);
    cmd.AddValue("bottleneckDataRate", "Data rate of the bottleneck link", bottleneckDataRate);
    cmd.AddValue("bottleneckDelay", "Delay of the bottleneck link", bottleneckDelay);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    // Create bottleneck link
    PointToPointHelper bottleneckP2p;
    bottleneckP2p.SetDeviceAttribute("DataRate", StringValue(bottleneckDataRate));
    bottleneckP2p.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    // Install network devices
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = bottleneckP2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices34 = p2p.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer devices35 = p2p.Install(nodes.Get(3), nodes.Get(5));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces34 = address.Assign(devices34);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces35 = address.Assign(devices35);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure TCP options
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(initialCwnd));
    if (enablePacing) {
        Config::SetDefault("ns3::TcpSocketBase::UseTcpPacing", BooleanValue(true));
        NS_LOG_INFO("TCP pacing enabled.");
    } else {
        Config::SetDefault("ns3::TcpSocketBase::UseTcpPacing", BooleanValue(false));
        NS_LOG_INFO("TCP pacing disabled.");
    }

    // Create traffic flow (TCP client on node 0, server on node 4)
    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(interfaces34.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(4));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    // Create BulkSendApplication
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces34.GetAddress(1), port));
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(1400));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send unlimited data
    ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime));

    //FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

    monitor->SerializeToXmlFile("tcp-pacing.flowmon", true, true);

    // Cleanup
    Simulator::Destroy();
    return 0;
}