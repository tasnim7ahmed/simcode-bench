#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControlComparison");

int
main(int argc, char *argv[])
{
    // Enable logging for applications (if needed)
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Command line parameters
    uint32_t maxBytes = 0; // Unlimited
    uint32_t packetSize = 1448;
    double simTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: n0 and n1 are TCP sources, n2 is the bottleneck/sink
    NodeContainer nodes;
    nodes.Create(3); // n0, n1, n2

    // Point-to-point links
    PointToPointHelper p2pAccess, p2pBottleneck;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("2ms"));

    // Bottleneck is slow link
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    // Connections
    // n0 <---p2pAccess---> n2
    // n1 <---p2pAccess---> n2
    NetDeviceContainer dev_n0n2 = p2pAccess.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev_n1n2 = p2pAccess.Install(nodes.Get(1), nodes.Get(2));

    // Bottleneck side (not needed, as n2 is the only receiver)
    // Could make a classic dumbbell if there were two sink nodes past n2

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n2 = ipv4.Assign(dev_n0n2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n2 = ipv4.Assign(dev_n1n2);

    // Traffic Control (queue) on bottleneck
    // Use DropTail Queue
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc", "MaxSize", StringValue("100p"));
    tch.Install(dev_n0n2.Get(1));
    tch.Install(dev_n1n2.Get(1));

    // Install sinks on n2, listening on different ports
    uint16_t port1 = 50000;
    uint16_t port2 = 50001;

    Address sinkAddress1(InetSocketAddress(if_n0n2.GetAddress(1), port1));
    Address sinkAddress2(InetSocketAddress(if_n1n2.GetAddress(1), port2));

    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApp1 = sinkHelper1.Install(nodes.Get(2));

    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp2 = sinkHelper2.Install(nodes.Get(2));

    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(simTime + 1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(simTime + 1));

    // TCP Source 1: Reno
    TypeId tcpRenoTypeId = TypeId::LookupByName("ns3::TcpReno");
    Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", TypeIdValue(tcpRenoTypeId));
    BulkSendHelper source1("ns3::TcpSocketFactory", sinkAddress1);
    source1.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    source1.SetAttribute("SendSize", UintegerValue(packetSize));
    ApplicationContainer sourceApp1 = source1.Install(nodes.Get(0));
    sourceApp1.Start(Seconds(1.0));
    sourceApp1.Stop(Seconds(simTime));

    // TCP Source 2: Cubic
    TypeId tcpCubicTypeId = TypeId::LookupByName("ns3::TcpCubic");
    Config::Set("/NodeList/1/$ns3::TcpL4Protocol/SocketType", TypeIdValue(tcpCubicTypeId));
    BulkSendHelper source2("ns3::TcpSocketFactory", sinkAddress2);
    source2.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    source2.SetAttribute("SendSize", UintegerValue(packetSize));
    ApplicationContainer sourceApp2 = source2.Install(nodes.Get(1));
    sourceApp2.Start(Seconds(1.0));
    sourceApp2.Stop(Seconds(simTime));

    // Enable pcap tracing
    p2pAccess.EnablePcapAll("congestion-control");

    // Animation
    AnimationInterface anim("congestion-control.xml");
    // Set node positions for clarity in NetAnim
    anim.SetConstantPosition(nodes.Get(0), 10.0, 30.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 60.0);
    anim.SetConstantPosition(nodes.Get(2), 50.0, 45.0);

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}