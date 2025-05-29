#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    // Set up point-to-point connections
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link between node 0 and node 1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Link between node 1 and node 2
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    // Set up UDP server (sink) on node 2
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client on node 0 sending to node 2
    UdpClientHelper client(i1i2.GetAddress(1), port); // node 2's IP address
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.03125))); // roughly 32pps
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing for all point-to-point devices
    p2p.EnablePcapAll("line-topology");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}