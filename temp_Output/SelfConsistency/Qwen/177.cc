#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControlComparison");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("TcpCongestionControlComparison", LOG_LEVEL_INFO);

    std::string tcpVariant = "TcpReno"; // Default TCP variant
    std::string simulationName = "TcpCongestionControlComparison";
    std::string animFile = "tcp-congestion.xml";

    // Allow user to override default TCP variant
    CommandLine cmd;
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpReno or TcpCubic", tcpVariant);
    cmd.Parse(argc, argv);

    if (tcpVariant == "TcpReno")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpReno::GetTypeId()));
    }
    else if (tcpVariant == "TcpCubic")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    }
    else
    {
        NS_FATAL_ERROR("Invalid TCP version. Use TcpReno or TcpCubic");
    }

    // Create nodes
    NodeContainer leftNodes;
    leftNodes.Create(2);
    NodeContainer rightNodes;
    rightNodes.Create(2);
    NodeContainer routers;
    routers.Create(2);

    // Connect nodes in dumbbell topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Left side links
    NetDeviceContainer leftRouterDevices;
    leftRouterDevices.Add(p2p.Install(leftNodes.Get(0), routers.Get(0)));
    leftRouterDevices.Add(p2p.Install(leftNodes.Get(1), routers.Get(0)));

    // Bottleneck link between routers
    p2p.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer bottleneckLink = p2p.Install(routers.Get(0), routers.Get(1));

    // Right side links
    NetDeviceContainer rightRouterDevices;
    rightRouterDevices.Add(p2p.Install(routers.Get(1), rightNodes.Get(0)));
    rightRouterDevices.Add(p2p.Install(routers.Get(1), rightNodes.Get(1)));

    // Install Internet stack
    InternetStackHelper internet;
    internet.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(leftRouterDevices);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(bottleneckLink);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ipv4.Assign(rightRouterDevices);

    // Set up applications
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(Ipv4Address("10.1.3.1"), sinkPort));
    Address sinkAddress2(InetSocketAddress(Ipv4Address("10.1.3.2"), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(rightNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    PacketSinkHelper packetSinkHelper2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps2 = packetSinkHelper2.Install(rightNodes.Get(1));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(10.0));

    // OnOffApplication for flow 1
    OnOffHelper onoff1("ns3::TcpSocketFactory", sinkAddress);
    onoff1.SetConstantRate(DataRate("1Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app1 = onoff1.Install(leftNodes.Get(0));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(9.0));

    // OnOffApplication for flow 2
    OnOffHelper onoff2("ns3::TcpSocketFactory", sinkAddress2);
    onoff2.SetConstantRate(DataRate("1Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer app2 = onoff2.Install(leftNodes.Get(1));
    app2.Start(Seconds(1.0));
    app2.Stop(Seconds(9.0));

    // Set up NetAnim visualization
    AnimationInterface anim(animFile);

    // Enable packet metadata for animation
    anim.EnablePacketMetadata(true);

    // Assign node names for animation
    anim.SetNodeDescription(leftNodes.Get(0), "Left-Client-1");
    anim.SetNodeDescription(leftNodes.Get(1), "Left-Client-2");
    anim.SetNodeDescription(routers.Get(0), "Left-Router");
    anim.SetNodeDescription(routers.Get(1), "Right-Router");
    anim.SetNodeDescription(rightNodes.Get(0), "Right-Server-1");
    anim.SetNodeDescription(rightNodes.Get(1), "Right-Server-2");

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}