#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer leftNodes, rightNodes, bottleneckNodes;
    leftNodes.Create(1);
    rightNodes.Create(1);
    bottleneckNodes.Create(2);

    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPointLeaf.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer leftDevices = pointToPointLeaf.Install(leftNodes.Get(0), bottleneckNodes.Get(0));
    NetDeviceContainer rightDevices = pointToPointLeaf.Install(rightNodes.Get(0), bottleneckNodes.Get(1));

    PointToPointHelper pointToPointBottleneck;
    pointToPointBottleneck.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    pointToPointBottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer bottleneckDevices = pointToPointBottleneck.Install(bottleneckNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces = address.Assign(leftDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces = address.Assign(rightDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces = address.Assign(bottleneckDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(0), port));
    source1.SetAttribute("SendSize", UintegerValue(1024));
    source1.SetAttribute("MaxBytes", UintegerValue(10000000));
    ApplicationContainer sourceApps1 = source1.Install(leftNodes.Get(0));
    sourceApps1.Start(Seconds(1.0));
    sourceApps1.Stop(Seconds(10.0));

    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(rightInterfaces.GetAddress(0), port + 1));
    source2.SetAttribute("SendSize", UintegerValue(1024));
    source2.SetAttribute("MaxBytes", UintegerValue(10000000));
    ApplicationContainer sourceApps2 = source2.Install(bottleneckNodes.Get(0));
    sourceApps2.Start(Seconds(1.0));
    sourceApps2.Stop(Seconds(10.0));


    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(rightNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer sinkApps2 = sink2.Install(rightNodes.Get(0));
    sinkApps2.Start(Seconds(0.0));
    sinkApps2.Stop(Seconds(10.0));


    TrafficControlHelper pfifoQdisc;
    uint16_t queueDiscId = pfifoQdisc.Install(bottleneckDevices).Get(0)->GetId();

    AnimationInterface anim("congestion.anim");
    anim.SetConstantPosition(leftNodes.Get(0), 0.0, 1.0);
    anim.SetConstantPosition(bottleneckNodes.Get(0), 2.0, 1.0);
    anim.SetConstantPosition(bottleneckNodes.Get(1), 4.0, 1.0);
    anim.SetConstantPosition(rightNodes.Get(0), 6.0, 1.0);


    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}