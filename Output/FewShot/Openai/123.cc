#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for UdpEcho apps
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Label nodes for easy reference
    Ptr<Node> n0 = nodes.Get(0); // Node 0
    Ptr<Node> n1 = nodes.Get(1); // Node 1
    Ptr<Node> n2 = nodes.Get(2); // Node 2
    Ptr<Node> n3 = nodes.Get(3); // Node 3

    // Create point-to-point links for the square topology
    // n0-n1, n1-n3, n3-n2, n2-n0
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // n0-n1
    NodeContainer n0n1(n0, n1);
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // n1-n3
    NodeContainer n1n3(n1, n3);
    NetDeviceContainer d1d3 = p2p.Install(n1n3);

    // n3-n2
    NodeContainer n3n2(n3, n2);
    NetDeviceContainer d3d2 = p2p.Install(n3n2);

    // n2-n0
    NodeContainer n2n0(n2, n0);
    NetDeviceContainer d2d0 = p2p.Install(n2n0);

    // Install Internet Stack with OSPF
    OspfRoutingHelper ospf;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospf);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // n0-n1
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    // n1-n3
    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = address.Assign(d1d3);

    // n3-n2
    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i2 = address.Assign(d3d2);

    // n2-n0
    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i0 = address.Assign(d2d0);

    // Enable pcap tracing on all point-to-point devices
    p2p.EnablePcap("n0-n1", d0d1, true);
    p2p.EnablePcap("n1-n3", d1d3, true);
    p2p.EnablePcap("n3-n2", d3d2, true);
    p2p.EnablePcap("n2-n0", d2d0, true);

    // Set up UDP communication between n0 (client) and n3 (server)
    uint16_t port = 9;

    // UDP Server on n3
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(n3);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(11.0));

    // UDP Client on n0, send 5 packets, every second, packet size 1024 bytes
    // Find n3's address for the client
    Ipv4Address n3Addr;
    // Choose n3's side of n1-n3 link (to be sure it's routable)
    n3Addr = i1i3.GetAddress(1);

    UdpEchoClientHelper echoClient(n3Addr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(n0);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(11.0));

    // Start simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    // Print routing tables at the end
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4GlobalRoutingHelper globalRouting;
    ospf.PrintRoutingTableAllAt(Seconds(11.5), std::cout);

    Simulator::Destroy();
    return 0;
}