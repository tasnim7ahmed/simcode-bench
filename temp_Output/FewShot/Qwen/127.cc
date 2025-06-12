#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-ospf-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for OSPF and routing
    LogComponentEnable("Ipv4OspfRouting", LOG_LEVEL_INFO);

    // Create 4 nodes representing routers
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between routers: 0-1, 1-2, 1-3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev13 = p2p.Install(nodes.Get(1), nodes.Get(3));

    // Install internet stack on all nodes
    InternetStackHelper stack;

    // Enable OSPFv2 on all routers
    Ipv4OspfRoutingHelper ospfRouting;
    stack.SetRoutingHelper(ospfRouting);  // has to be set before installing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = ip.Assign(dev01);
    ip.NewNetwork();
    Ipv4InterfaceContainer if12 = ip.Assign(dev12);
    ip.NewNetwork();
    Ipv4InterfaceContainer if13 = ip.Assign(dev13);

    // Enable global static routing so that the helper can discover all interfaces
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up a UDP echo server on node 2 (destination)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up a UDP echo client on node 0 (source) sending packets every second
    UdpEchoClientHelper echoClient(if12.GetAddress(0), 9); // IP of node 2's interface
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing for all devices to observe routing packets and data traffic
    p2p.EnablePcapAll("ospf-link-state-routing");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}