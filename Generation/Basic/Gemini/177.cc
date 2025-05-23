#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("TcpReno", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("TcpCubic", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("BulkSendApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("DropTailQueue", ns3::LOG_LEVEL_INFO);

    double simulationTime = 30.0; // seconds

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // Create nodes for dumbbell topology
    // N0: Sender 1 (Reno)
    // N1: Sender 2 (Cubic)
    // N2: Left Router
    // N3: Right Router
    // N4: Receiver 1 (for Reno)
    // N5: Receiver 2 (for Cubic)
    ns3::NodeContainer nodes;
    nodes.Create(6);

    ns3::Ptr<ns3::Node> s1 = nodes.Get(0);
    ns3::Ptr<ns3::Node> s2 = nodes.Get(1);
    ns3::Ptr<ns3::Node> r1 = nodes.Get(2);
    ns3::Ptr<ns3::Node> r2 = nodes.Get(3);
    ns3::Ptr<ns3::Node> d1 = nodes.Get(4);
    ns3::Ptr<ns3::Node> d2 = nodes.Get(5);

    // Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Configure Point-to-Point links
    ns3::PointToPointHelper p2pHelper;

    // Link: Sender 1 (N0) -- Left Router (N2)
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("1ms"));
    ns3::NetDeviceContainer devices_s1_r1 = p2pHelper.Install(s1, r1);
    ns3::Ipv4AddressHelper ipv4_s1_r1;
    ipv4_s1_r1.SetBase("10.0.0.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_s1_r1 = ipv4_s1_r1.Assign(devices_s1_r1);

    // Link: Sender 2 (N1) -- Left Router (N2)
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("1ms"));
    ns3::NetDeviceContainer devices_s2_r1 = p2pHelper.Install(s2, r1);
    ns3::Ipv4AddressHelper ipv4_s2_r1;
    ipv4_s2_r1.SetBase("10.0.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_s2_r1 = ipv4_s2_r1.Assign(devices_s2_r1);

    // Link: Left Router (N2) -- Right Router (N3) - Bottleneck Link
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("50ms"));
    p2pHelper.SetQueue("ns3::DropTailQueue", "MaxBytes", ns3::StringValue("150KB"));
    ns3::NetDeviceContainer devices_r1_r2 = p2pHelper.Install(r1, r2);
    ns3::Ipv4AddressHelper ipv4_r1_r2;
    ipv4_r1_r2.SetBase("10.0.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_r1_r2 = ipv4_r1_r2.Assign(devices_r1_r2);

    // Link: Right Router (N3) -- Receiver 1 (N4)
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("1ms"));
    p2pHelper.SetQueue("ns3::DropTailQueue"); // Reset queue settings for access links
    ns3::NetDeviceContainer devices_r2_d1 = p2pHelper.Install(r2, d1);
    ns3::Ipv4AddressHelper ipv4_r2_d1;
    ipv4_r2_d1.SetBase("10.0.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_r2_d1 = ipv4_r2_d1.Assign(devices_r2_d1);

    // Link: Right Router (N3) -- Receiver 2 (N5)
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("1ms"));
    ns3::NetDeviceContainer devices_r2_d2 = p2pHelper.Install(r2, d2);
    ns3::Ipv4AddressHelper ipv4_r2_d2;
    ipv4_r2_d2.SetBase("10.0.4.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_r2_d2 = ipv4_r2_d2.Assign(devices_r2_d2);

    // Populate routing tables
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure TCP variants
    ns3::Config::SetDefault("ns3::TcpSocket::InitialCwnd", ns3::UintegerValue(1));

    // Configure TCP Reno for Sender 1 (N0)
    ns3::Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionControlAlgorithm", ns3::StringValue("ns3::TcpReno"));

    // Configure TCP Cubic for Sender 2 (N1)
    ns3::Config::Set("/NodeList/1/$ns3::TcpL4Protocol/CongestionControlAlgorithm", ns3::StringValue("ns3::TcpCubic"));


    // Install Applications
    ns3::uint16 port = 9;

    // Flow 1: Reno (Sender 1 to Receiver 1)
    ns3::PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer sinkApp1 = sinkHelper1.Install(d1);
    sinkApp1.Start(ns3::Seconds(0.0));
    sinkApp1.Stop(ns3::Seconds(simulationTime));

    ns3::BulkSendHelper sourceHelper1("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(interfaces_r2_d1.GetAddress(0), port));
    sourceHelper1.SetAttribute("MaxBytes", ns3::UintegerValue(0));
    ns3::ApplicationContainer sourceApp1 = sourceHelper1.Install(s1);
    sourceApp1.Start(ns3::Seconds(1.0));
    sourceApp1.Stop(ns3::Seconds(simulationTime - 1.0));

    // Flow 2: Cubic (Sender 2 to Receiver 2)
    ns3::PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer sinkApp2 = sinkHelper2.Install(d2);
    sinkApp2.Start(ns3::Seconds(0.0));
    sinkApp2.Stop(ns3::Seconds(simulationTime));

    ns3::BulkSendHelper sourceHelper2("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(interfaces_r2_d2.GetAddress(0), port));
    sourceHelper2.SetAttribute("MaxBytes", ns3::UintegerValue(0));
    ns3::ApplicationContainer sourceApp2 = sourceHelper2.Install(s2);
    sourceApp2.Start(ns3::Seconds(1.0));
    sourceApp2.Stop(ns3::Seconds(simulationTime - 1.0));


    // NetAnim Visualization
    ns3::AnimationInterface anim("dumbbell-tcp.xml");

    anim.SetConstantPosition(s1, 10.0, 50.0);
    anim.SetConstantPosition(s2, 10.0, 150.0);
    anim.SetConstantPosition(r1, 50.0, 100.0);
    anim.SetConstantPosition(r2, 150.0, 100.0);
    anim.SetConstantPosition(d1, 190.0, 50.0);
    anim.SetConstantPosition(d2, 190.0, 150.0);

    anim.EnablePacketMetadata();
    anim.EnableIpv4RouteTracking();
    anim.EnableQueuesTracking();


    // Flow Monitor
    ns3::FlowMonitorHelper flowMonitor;
    ns3::Ptr<ns3::FlowMonitor> monitor = flowMonitor.InstallAll();

    // Run Simulation
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();

    // Flow Monitor Statistics
    monitor->SerializeToXmlFile("dumbbell-tcp-flowmon.xml", true, true);

    ns3::Simulator::Destroy();

    return 0;
}