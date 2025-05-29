#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpNonPersistentNetAnim");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetPrintPrefix(true);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;

    // Server
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Client
    OnOffHelper onOffHelper("ns3::TcpSocketFactory",
                            Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("DataRate", StringValue("1Mbps")); // Ensure DataRate <= Bandwidth

    ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(2.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AnimationInterface anim("tcp-non-persistent-netanim.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 100, 10);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}