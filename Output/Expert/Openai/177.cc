#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Nodes: n0 (left), n1 (bottleneck), n2 (right)
    NodeContainer leftNodes;
    leftNodes.Create(2); // n0, n2

    NodeContainer router;
    router.Create(1); // n1

    // n0 <-> n1 (left)
    NodeContainer n0n1 = NodeContainer(leftNodes.Get(0), router.Get(0));
    // n2 <-> n1 (right)
    NodeContainer n2n1 = NodeContainer(leftNodes.Get(1), router.Get(0));

    // Point-to-point links
    PointToPointHelper p2pFast;
    p2pFast.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pFast.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("20ms"));

    // NetDevice containers
    NetDeviceContainer d0r, d2r;

    d0r = p2pFast.Install(n0n1);
    d2r = p2pBottleneck.Install(n2n1);

    // Assign IP addresses
    InternetStackHelper internet;
    internet.Install(leftNodes);
    internet.Install(router);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0r);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i1 = ipv4.Assign(d2r);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install TCP applications
    uint16_t sinkPort1 = 8080;
    uint16_t sinkPort2 = 8090;

    // TCP Flow 1: Reno, n0 (src) -> n2 (dst)
    Address sinkAddress1(InetSocketAddress(i2i1.GetAddress(0), sinkPort1));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkAddress1);
    ApplicationContainer sinkApp1 = sinkHelper1.Install(leftNodes.Get(1)); // n2

    // TCP Flow 2: Cubic, n2 (src) -> n0 (dst)
    Address sinkAddress2(InetSocketAddress(i0i1.GetAddress(0), sinkPort2));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkAddress2);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(leftNodes.Get(0)); // n0

    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(20.0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(20.0));

    // Flow 1 source: n0 (Reno)
    Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpReno::GetTypeId()));
    OnOffHelper onoff1("ns3::TcpSocketFactory", sinkAddress1);
    onoff1.SetAttribute("PacketSize", UintegerValue(1440));
    onoff1.SetAttribute("DataRate", StringValue("50Mbps"));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(19.0)));

    ApplicationContainer srcApps1 = onoff1.Install(leftNodes.Get(0)); // n0

    // Flow 2 source: n2 (Cubic)
    Config::Set("/NodeList/1/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    OnOffHelper onoff2("ns3::TcpSocketFactory", sinkAddress2);
    onoff2.SetAttribute("PacketSize", UintegerValue(1440));
    onoff2.SetAttribute("DataRate", StringValue("50Mbps"));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(1.2)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(19.2)));

    ApplicationContainer srcApps2 = onoff2.Install(leftNodes.Get(1)); // n2

    // NetAnim setup
    AnimationInterface anim("tcp-congestion-dumbbell.xml");
    anim.SetConstantPosition(leftNodes.Get(0), 10.0, 30.0);
    anim.SetConstantPosition(router.Get(0), 50.0, 30.0);
    anim.SetConstantPosition(leftNodes.Get(1), 90.0, 30.0);

    // Bottleneck identification
    anim.UpdateNodeDescription(leftNodes.Get(0), "n0 (Reno Src)");
    anim.UpdateNodeDescription(router.Get(0), "n1 (Router, Bottleneck)");
    anim.UpdateNodeDescription(leftNodes.Get(1), "n2 (Cubic Src, Sinks)");
    anim.UpdateNodeColor(router.Get(0), 255, 0, 0); // Highlight bottleneck

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("tcp-bottleneck-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}