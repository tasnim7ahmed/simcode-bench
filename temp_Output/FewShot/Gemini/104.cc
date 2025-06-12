#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/pcap-file.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3); // A, B, C

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routingA = staticRouting.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    routingA->AddHostRouteTo(Ipv4Address("10.1.2.2"), interfacesAB.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> routingB = staticRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    routingB->AddHostRouteTo(Ipv4Address("10.1.1.1"), interfacesAB.GetAddress(1), 1);
    routingB->AddHostRouteTo(Ipv4Address("10.1.2.2"), interfacesBC.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> routingC = staticRouting.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    routingC->AddHostRouteTo(Ipv4Address("10.1.1.1"), interfacesBC.GetAddress(1), 1);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    ApplicationContainer onoffApp = onoff.Install(nodes.Get(0));
    onoffApp.Start(Seconds(1.0));
    onoffApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    p2p.EnablePcapAll("routing", false);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}