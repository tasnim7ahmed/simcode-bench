#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControlDumbbell");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    std::string tcpVariant = "TcpReno";
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpReno or TcpCubic", tcpVariant);
    cmd.Parse(argc, argv);

    if (tcpVariant == "TcpReno") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpReno::GetTypeId()));
    } else if (tcpVariant == "TcpCubic") {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    }

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer leftNodes;
    leftNodes.Create(2);

    NodeContainer rightNodes;
    rightNodes.Create(2);

    NodeContainer bottleneck;
    bottleneck.Add(leftNodes.Get(0));
    bottleneck.Add(rightNodes.Get(0));

    PointToPointHelper p2pLeft;
    p2pLeft.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLeft.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    PointToPointHelper p2pRight;
    p2pRight.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRight.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer leftDevices[2], rightDevices[2], bottleneckDevices;

    for (uint32_t i = 0; i < 2; ++i) {
        leftDevices[i] = p2pLeft.Install(leftNodes.Get(i), bottleneck.Get(0));
    }

    bottleneckDevices = p2pBottleneck.Install(bottleneck);

    for (uint32_t i = 0; i < 2; ++i) {
        rightDevices[i] = p2pRight.Install(rightNodes.Get(i), bottleneck.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces[2];
    for (uint32_t i = 0; i < 2; ++i) {
        leftInterfaces[i] = address.Assign(leftDevices[i]);
        address.NewNetwork();
    }

    address.SetBase("10.2.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);
    address.NewNetwork();

    address.SetBase("10.3.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces[2];
    for (uint32_t i = 0; i < 2; ++i) {
        rightInterfaces[i] = address.Assign(rightDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    OnOffHelper onoff1("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces[0].GetAddress(1), port));
    onoff1.SetConstantRate(DataRate("2Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer apps1 = onoff1.Install(leftNodes.Get(0));
    apps1.Start(Seconds(1.0));
    apps1.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces[1].GetAddress(1), port));
    onoff2.SetConstantRate(DataRate("2Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer apps2 = onoff2.Install(leftNodes.Get(1));
    apps2.Start(Seconds(1.0));
    apps2.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps1 = sink.Install(rightNodes.Get(0));
    sinkApps1.Start(Seconds(0.0));
    ApplicationContainer sinkApps2 = sink.Install(rightNodes.Get(1));
    sinkApps2.Start(Seconds(0.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tcp-congestion.tr");
    p2pBottleneck.EnableAsciiAll(stream);

    p2pLeft.EnablePcapAll("tcp-congestion-left");
    p2pBottleneck.EnablePcapAll("tcp-congestion-bottleneck");
    p2pRight.EnablePcapAll("tcp-congestion-right");

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(NodeContainer::GetGlobal());

    AnimationInterface anim("tcp-congestion.xml");
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}