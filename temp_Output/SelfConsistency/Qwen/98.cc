#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedRoutingSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(7);

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // CSMA links
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Build network topology

    // Point-to-point links: node 0 <-> 1 <-> 2 <-> 6
    NetDeviceContainer p2p0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer p2p1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer p2p2_6 = p2p.Install(nodes.Get(2), nodes.Get(6));

    // Alternative point-to-point link: node 0 <-> 3 <-> 4 <-> 6
    NetDeviceContainer p2p0_3 = p2p.Install(nodes.Get(0), nodes.Get(3));
    NetDeviceContainer p2p3_4 = p2p.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer p2p4_6 = p2p.Install(nodes.Get(4), nodes.Get(6));

    // CSMA links between intermediate nodes
    NetDeviceContainer csma2_3 = csma.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));
    NetDeviceContainer csma3_5 = csma.Install(NodeContainer(nodes.Get(3), nodes.Get(5)));
    NetDeviceContainer csma5_4 = csma.Install(NodeContainer(nodes.Get(5), nodes.Get(4)));

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ip0_1 = address.Assign(p2p0_1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ip1_2 = address.Assign(p2p1_2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ip2_6 = address.Assign(p2p2_6);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ip0_3 = address.Assign(p2p0_3);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ip3_4 = address.Assign(p2p3_4);

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ip4_6 = address.Assign(p2p4_6);

    address.SetBase("10.1.7.0", "255.255.255.0");
    address.Assign(csma2_3);

    address.SetBase("10.1.8.0", "255.255.255.0");
    address.Assign(csma3_5);

    address.SetBase("10.1.9.0", "255.255.255.0");
    address.Assign(csma5_4);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create CBR/UDP flows from node 1 to node 6
    uint16_t port = 9;

    // First flow: starts at 1s
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(6));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient1(ip4_6.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(200));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(10.0));

    // Second flow: starts at 11s
    UdpEchoClientHelper echoClient2(ip4_6.GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(200));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(11.0));
    clientApp2.Stop(Seconds(20.0));

    // Schedule interface down and up events for rerouting
    Simulator::Schedule(Seconds(5.0), &Ipv4InterfaceContainer::SetDown, &ip1_2, 1);
    Simulator::Schedule(Seconds(10.0), &Ipv4InterfaceContainer::SetUp, &ip1_2, 1);

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-routing.tr");
    p2p.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    // Enable pcap tracing on key devices
    p2p.EnablePcapAll("mixed-routing");
    csma.EnablePcapAll("mixed-routing");

    // Schedule route recomputation after interface changes
    Simulator::Schedule(Seconds(5.1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}