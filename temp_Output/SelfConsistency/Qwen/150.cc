#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LineTopologyUdpSimulation");

int main(int argc, char *argv[]) {
    // Log component for debugging and output
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between:
    // Node 0 <-> Node 1
    // Node 1 <-> Node 2
    // Node 2 <-> Node 3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer link0, link1, link2;

    link0 = p2p.Install(nodes.Get(0), nodes.Get(1));
    link1 = p2p.Install(nodes.Get(1), nodes.Get(2));
    link2 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each network segment
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0 = address.Assign(link0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(link1);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(link2);

    // Enable global routing so packets can find their way end-to-end
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP echo server on node 3 (port 9 by default)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP echo client on node 0, sending to node 3's IP
    UdpEchoClientHelper echoClient(interfaces2.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing for all devices
    p2p.EnablePcapAll("line_topology_udp");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}