#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 7 nodes
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

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create topology

    // P2P links: 0-1, 1-2, 2-6, 1-3, 3-6
    NetDeviceContainer dev0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev2_6 = p2p.Install(nodes.Get(2), nodes.Get(6));
    NetDeviceContainer dev1_3 = p2p.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer dev3_6 = p2p.Install(nodes.Get(3), nodes.Get(6));

    // CSMA links: 0-4, 4-5, 5-6
    NetDeviceContainer dev0_4 = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(4)));
    NetDeviceContainer dev4_5 = csma.Install(NodeContainer(nodes.Get(4), nodes.Get(5)));
    NetDeviceContainer dev5_6 = csma.Install(NodeContainer(nodes.Get(5), nodes.Get(6)));

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer if0_1 = ip.Assign(dev0_1);
    Ipv4InterfaceContainer if1_2 = ip.Assign(dev1_2);
    Ipv4InterfaceContainer if2_6 = ip.Assign(dev2_6);
    Ipv4InterfaceContainer if1_3 = ip.Assign(dev1_3);
    Ipv4InterfaceContainer if3_6 = ip.Assign(dev3_6);
    Ipv4InterfaceContainer if0_4 = ip.Assign(dev0_4);
    Ipv4InterfaceContainer if4_5 = ip.Assign(dev4_5);
    Ipv4InterfaceContainer if5_6 = ip.Assign(dev5_6);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create CBR/UDP applications on node 1 sending to node 6

    uint16_t port = 9;

    // First flow (starts at 1s)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(6));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient1(if5_6.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(20.0));

    // Second flow (starts at 11s)
    UdpEchoClientHelper echoClient2(if3_6.GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(11.0));
    clientApp2.Stop(Seconds(20.0));

    // Interface down/up events
    Simulator::Schedule(Seconds(3.0), &Ipv4InterfaceContainer::SetDown, &if1_2, 0);
    Simulator::Schedule(Seconds(5.0), &Ipv4InterfaceContainer::SetUp, &if1_2, 0);

    Simulator::Schedule(Seconds(8.0), &Ipv4InterfaceContainer::SetDown, &if1_3, 0);
    Simulator::Schedule(Seconds(10.0), &Ipv4InterfaceContainer::SetUp, &if1_3, 0);

    // Schedule global route recomputation after interface changes
    Simulator::Schedule(Seconds(3.1), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables);

    // Enable tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("interface-down-up-packet-trace.tr"));
    csma.EnableAsciiAll(ascii.CreateFileStream("csma-packet-trace.tr"));

    pointToPoint.EnablePcapAll("interface-down-up");
    csma.EnablePcapAll("csma");

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}