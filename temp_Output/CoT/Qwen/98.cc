#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedGlobalRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(7);

    // Point-to-point links: node0-node1, node1-node2, node2-node3, node3-node6
    // CSMA links: node0-node4, node4-node5, node5-node6

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.01)));

    NetDeviceContainer p2pDevices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer p2pDevices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer p2pDevices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer p2pDevices36 = p2p.Install(nodes.Get(3), nodes.Get(6));

    NetDeviceContainer csmaDevices04 = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(4)));
    NetDeviceContainer csmaDevices45 = csma.Install(NodeContainer(nodes.Get(4), nodes.Get(5)));
    NetDeviceContainer csmaDevices56 = csma.Install(NodeContainer(nodes.Get(5), nodes.Get(6)));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces01 = address.Assign(p2pDevices01);
    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces12 = address.Assign(p2pDevices12);
    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces23 = address.Assign(p2pDevices23);
    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces36 = address.Assign(p2pDevices36);
    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces04 = address.Assign(csmaDevices04);
    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces45 = address.Assign(csmaDevices45);
    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces56 = address.Assign(csmaDevices56);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(6));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));

    CbrHelper cbr("ns3::UdpSocketFactory", InetSocketAddress(csmaInterfaces56.GetAddress(1), port));
    cbr.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    cbr.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = cbr.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(20.0));

    ApplicationContainer clientApp2 = cbr.Install(nodes.Get(1));
    clientApp2.Start(Seconds(11.0));
    clientApp2.Stop(Seconds(20.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("mixed-routing-packet-trace.tr");
    p2p.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    Simulator::Schedule(Seconds(4.0), &Ipv4InterfaceContainer::SetDown, &p2pInterfaces01, 1);
    Simulator::Schedule(Seconds(8.0), &Ipv4InterfaceContainer::SetUp, &p2pInterfaces01, 1);

    Simulator::Schedule(Seconds(6.0), &Ipv4InterfaceContainer::SetDown, &p2pInterfaces23, 0);
    Simulator::Schedule(Seconds(10.0), &Ipv4InterfaceContainer::SetUp, &p2pInterfaces23, 0);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}