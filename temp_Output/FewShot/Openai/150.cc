#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes in a line topology
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between adjacent nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Node 0 <-> Node 1
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    // Node 1 <-> Node 2
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    // Node 2 <-> Node 3
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each network segment
    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address01.Assign(devices01);

    Ipv4AddressHelper address12;
    address12.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address12.Assign(devices12);

    Ipv4AddressHelper address23;
    address23.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address23.Assign(devices23);

    // Enable PCAP tracing for all point-to-point devices
    p2p.EnablePcapAll("line-topology");

    // Set up UDP Echo Server on node 3
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0 targeting node 3
    Ipv4Address destAddr = interfaces23.GetAddress(1); // Node 3's interface on last link
    UdpEchoClientHelper echoClient(destAddr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}