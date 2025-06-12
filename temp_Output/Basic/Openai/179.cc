#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Set TCP variant to NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    NodeContainer nodes;
    nodes.Create(2);

    // Set up the point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    // Set up the BulkSendApplication (source)
    BulkSendHelper source("ns3::TcpSocketFactory", 
        InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data

    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // Set up PacketSinkApplication (sink)
    PacketSinkHelper sink("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(11.0));

    // Enable NetAnim
    AnimationInterface anim("p2p-tcp-newreno.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 20.0);
    anim.SetConstantPosition(nodes.Get(1), 40.0, 20.0);

    // Enable packet metadata for animation
    pointToPoint.EnablePcapAll("p2p-tcp-newreno", true);

    // FlowMonitor for statistics (optional, but useful for packet flow display)
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("p2p-tcp-newreno-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}