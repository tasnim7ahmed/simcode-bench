#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links: node 0 <-> node 1 and node 0 <-> node 2
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev01 = p2p.Install(n0n1);
    NetDeviceContainer dev02 = p2p.Install(n0n2);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    // Assign IP addresses
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface01 = address.Assign(dev01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface02 = address.Assign(dev02);

    // UDP Echo Server on node 1
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on node 2, targeting the server on node 1 (through node 0)
    UdpEchoClientHelper echoClient(iface01.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(2));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable PCAP tracing
    p2p.EnablePcapAll("branch-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}