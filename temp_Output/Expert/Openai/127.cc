#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ospf-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer routers;
    routers.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer d12 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer d23 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer d03 = p2p.Install(routers.Get(0), routers.Get(3));

    InternetStackHelper stack;
    OspfRoutingHelper ospf;
    stack.SetRoutingHelper(ospf);
    stack.Install(routers);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = ipv4.Assign(d01);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = ipv4.Assign(d12);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i03 = ipv4.Assign(d03);

    p2p.EnablePcapAll("ospf-sim", true);

    uint16_t port = 50000;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(i23.GetAddress(1), port)));
    onoff.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    ApplicationContainer app = onoff.Install(routers.Get(0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer sinkApp = sink.Install(routers.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(12.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}