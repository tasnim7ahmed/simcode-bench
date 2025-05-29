#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ospf-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create four router nodes
    NodeContainer routers;
    routers.Create(4);

    // Define point-to-point channel attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect routers in a mesh: 0-1, 0-2, 1-2, 1-3, 2-3
    NodeContainer n0n1 = NodeContainer(routers.Get(0), routers.Get(1));
    NodeContainer n0n2 = NodeContainer(routers.Get(0), routers.Get(2));
    NodeContainer n1n2 = NodeContainer(routers.Get(1), routers.Get(2));
    NodeContainer n1n3 = NodeContainer(routers.Get(1), routers.Get(3));
    NodeContainer n2n3 = NodeContainer(routers.Get(2), routers.Get(3));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);
    NetDeviceContainer d1d3 = p2p.Install(n1n3);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // Install OSPF-enabled Internet stack
    OspfRoutingHelper ospf;
    Ipv4ListRoutingHelper list;
    list.Add(ospf, 100);
    InternetStackHelper stack;
    stack.SetRoutingHelper(list);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = address.Assign(d1d3);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    // Enable pcap tracing (on all router devices)
    p2p.EnablePcapAll("ospf-example", true);

    // Set up OnOff UDP application: router 0 sends to router 3
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(i1i3.GetAddress(1), port));
    onoff.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(14.0)));

    // Install OnOff on router 0
    ApplicationContainer app = onoff.Install(routers.Get(0));

    // Set up UDP packet sink on router 3
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(routers.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(16.0));

    // Enable ASCII tracing if desired
    // AsciiTraceHelper ascii;
    // p2p.EnableAsciiAll(ascii.CreateFileStream("ospf-example.tr"));

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}