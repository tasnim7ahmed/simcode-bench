#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(7);

    // Point-to-point links
    PointToPointHelper p2p1, p2p2, p2p3, p2p4, p2p5;
    p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("2ms"));

    p2p2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("2ms"));

    p2p3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p3.SetChannelAttribute("Delay", StringValue("2ms"));

    p2p4.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p4.SetChannelAttribute("Delay", StringValue("2ms"));

    p2p5.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p5.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p1.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1d2 = p2p2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2d3 = p2p3.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d3d4 = p2p4.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer d4d5 = p2p5.Install(nodes.Get(4), nodes.Get(5));

    // CSMA links between intermediate nodes
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    NetDeviceContainer d0d2 = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));
    NetDeviceContainer d2d4 = csma.Install(NodeContainer(nodes.Get(2), nodes.Get(4)));
    NetDeviceContainer d4d6 = csma.Install(NodeContainer(nodes.Get(4), nodes.Get(6)));
    NetDeviceContainer d1d3 = csma.Install(NodeContainer(nodes.Get(1), nodes.Get(3)));
    NetDeviceContainer d3d5 = csma.Install(NodeContainer(nodes.Get(3), nodes.Get(5)));
    NetDeviceContainer d5d6 = csma.Install(NodeContainer(nodes.Get(5), nodes.Get(6)));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);
    address.NewNetwork();
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);
    address.NewNetwork();
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);
    address.NewNetwork();
    Ipv4InterfaceContainer i3i4 = address.Assign(d3d4);
    address.NewNetwork();
    Ipv4InterfaceContainer i4i5 = address.Assign(d4d5);
    address.NewNetwork();
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);
    address.NewNetwork();
    Ipv4InterfaceContainer i2i4 = address.Assign(d2d4);
    address.NewNetwork();
    Ipv4InterfaceContainer i4i6 = address.Assign(d4d6);
    address.NewNetwork();
    Ipv4InterfaceContainer i1i3 = address.Assign(d1d3);
    address.NewNetwork();
    Ipv4InterfaceContainer i3i5 = address.Assign(d3d5);
    address.NewNetwork();
    Ipv4InterfaceContainer i5i6 = address.Assign(d5d6);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(6));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient1(i6i6.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(10000));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient2(i6i6.GetAddress(1), port + 1);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10000));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(11.0));
    clientApp2.Stop(Seconds(20.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("packet-trace.tr");
    stack.EnableAsciiIpv4All(stream);

    Simulator::Schedule(Seconds(5.0), &Ipv4InterfaceContainer::SetDown, &i1i2, 1);
    Simulator::Schedule(Seconds(8.0), &Ipv4InterfaceContainer::SetUp, &i1i2, 1);

    Simulator::Schedule(Seconds(15.0), &Ipv4InterfaceContainer::SetDown, &i3i4, 0);
    Simulator::Schedule(Seconds(18.0), &Ipv4InterfaceContainer::SetUp, &i3i4, 0);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}