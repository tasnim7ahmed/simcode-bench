#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for debugging (optional)
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links and device containers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Create 3 network segments: 0-1, 1-2, 2-3
    NetDeviceContainer dev01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface01 = address.Assign(dev01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface12 = address.Assign(dev12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iface23 = address.Assign(dev23);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP server on node 3
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on node 0 to send packets to node 3
    UdpEchoClientHelper client(iface23.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01))); // 10ms interval
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing on all point-to-point devices
    p2p.EnablePcapAll("line-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}