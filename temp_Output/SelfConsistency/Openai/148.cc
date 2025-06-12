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

    // Enable logging if needed
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: node0 is central, others are branches
    NodeContainer n0n1, n0n2, n0n3;
    Ptr<Node> node0 = CreateObject<Node>();
    Ptr<Node> node1 = CreateObject<Node>();
    Ptr<Node> node2 = CreateObject<Node>();
    Ptr<Node> node3 = CreateObject<Node>();

    n0n1.Add(node0);
    n0n1.Add(node1);

    n0n2.Add(node0);
    n0n2.Add(node2);

    n0n3.Add(node0);
    n0n3.Add(node3);

    // Install the Internet stack
    NodeContainer allNodes;
    allNodes.Add(node0);
    allNodes.Add(node1);
    allNodes.Add(node2);
    allNodes.Add(node3);
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d0d3 = p2p.Install(n0n3);

    // Assign unique IP addresses to each network segment
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i3 = ipv4.Assign(d0d3);

    // Install UDP server on node0
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(node0);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP clients on nodes 1, 2, 3
    UdpClientHelper client1(i0i1.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(320));
    client1.SetAttribute("Interval", TimeValue(Seconds(0.03)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));

    UdpClientHelper client2(i0i2.GetAddress(0), port);
    client2.SetAttribute("MaxPackets", UintegerValue(320));
    client2.SetAttribute("Interval", TimeValue(Seconds(0.03)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));

    UdpClientHelper client3(i0i3.GetAddress(0), port);
    client3.SetAttribute("MaxPackets", UintegerValue(320));
    client3.SetAttribute("Interval", TimeValue(Seconds(0.03)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = client1.Install(node1);
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = client2.Install(node2);
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(10.0));

    ApplicationContainer clientApps3 = client3.Install(node3);
    clientApps3.Start(Seconds(1.0));
    clientApps3.Stop(Seconds(10.0));

    // Enable PCAP tracing for all point-to-point devices
    p2p.EnablePcapAll("branch-topology");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}