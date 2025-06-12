#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CongestionControlComparison");

int main(int argc, char *argv[]) {
    std::string tcpVariant = "TcpReno";
    std::string simulationName = "congestion-control-comparison";

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpReno or TcpCubic", tcpVariant);
    cmd.Parse(argc, argv);

    if (tcpVariant == "TcpReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpReno::GetTypeId()));
    } else if (tcpVariant == "TcpCubic") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    }

    NodeContainer leftNodes;
    leftNodes.Create(2);

    NodeContainer rightNodes;
    rightNodes.Create(2);

    NodeContainer bottleneckNodes;
    bottleneckNodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer leftDevices[2], rightDevices[2];

    for (uint32_t i = 0; i < 2; ++i) {
        leftDevices[i] = p2p.Install(leftNodes.Get(i), bottleneckNodes.Get(0));
        rightDevices[i] = p2p.Install(bottleneckNodes.Get(1), rightNodes.Get(i));
    }

    p2p.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer bottleneckDevice = p2p.Install(bottleneckNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[5];

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(leftDevices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(leftDevices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(bottleneckDevice);
    address.NewNetwork();
    interfaces[3] = address.Assign(rightDevices[0]);
    address.NewNetwork();
    interfaces[4] = address.Assign(rightDevices[1]);

    uint16_t port = 50000;

    for (uint32_t i = 0; i < 2; ++i) {
        InetSocketAddress sinkAddr(interfaces[3 + i].GetAddress(0), port);
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
        ApplicationContainer sinkApp = sinkHelper.Install(rightNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));

        OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
        onoff.SetConstantRate(DataRate("2Mbps"));
        ApplicationContainer app = onoff.Install(leftNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(9.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim(simulationName + ".xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking(simulationName + "-routing.xml", Seconds(0), Seconds(10), Seconds(0.25));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        NS_LOG_INFO("Flow ID: " << iter->first << " Info: " << iter->second);
    }

    Simulator::Destroy();
    return 0;
}