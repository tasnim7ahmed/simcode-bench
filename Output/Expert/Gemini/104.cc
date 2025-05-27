#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(devicesBC);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routerA = staticRouting.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    routerA->AddHostRouteTo(Ipv4Address("10.1.2.2"), interfacesAB.GetAddress(0), 1);

    Ptr<Ipv4StaticRouting> routerB = staticRouting.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    routerB->AddHostRouteTo(Ipv4Address("10.1.1.1"), interfacesAB.GetAddress(1), 1);
    routerB->AddHostRouteTo(Ipv4Address("10.1.2.2"), interfacesBC.GetAddress(0), 1);

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfacesBC.GetAddress(1), 9)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));

    ApplicationContainer appSource = onoff.Install(nodes.Get(0));
    appSource.Start(Seconds(1.0));
    appSource.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer appSink = sink.Install(nodes.Get(2));
    appSink.Start(Seconds(1.0));
    appSink.Stop(Seconds(10.0));

    pointToPoint.EnablePcapAll("router");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}