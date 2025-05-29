#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestion");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetDefaultLogComponentEnable(true);

    NodeContainer nodes;
    nodes.Create(3);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPointLeaf.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer leftDevices, rightDevices;
    leftDevices = pointToPointLeaf.Install(nodes.Get(0), nodes.Get(1));
    rightDevices = pointToPointLeaf.Install(nodes.Get(2), nodes.Get(1));

    PointToPointHelper pointToPointBottleneck;
    pointToPointBottleneck.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPointBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer bottleneckDevices;
    bottleneckDevices = pointToPointBottleneck.Install(nodes.Get(1), nodes.Get(1));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces = address.Assign(leftDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces = address.Assign(rightDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;

    // Reno Application
    TcpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    TcpEchoClientHelper echoClient(rightInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    //Cubic application
    BulkSendHelper source("ns3::TcpSocketFactory",
                           InetSocketAddress(rightInterfaces.GetAddress(0), 50001));
    source.SetAttribute("SendSize", UintegerValue(1024));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(2.0));
    sourceApps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), 50001));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));


    // Set TCP variants
    GlobalValue::Bind("Simulator::DefaultTcpVariant", StringValue("ns3::TcpReno"));

    Simulator::Stop(Seconds(10.0));

    AnimationInterface anim("tcp-congestion.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 2.0);
    anim.SetConstantPosition(nodes.Get(1), 5.0, 2.0);
    anim.SetConstantPosition(nodes.Get(2), 10.0, 2.0);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}