#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopologyExample");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create three nodes: Node 0 (central), Node 1 and Node 2
    NodeContainer n0, n1, n2;
    n0.Create(1);
    n1.Create(1);
    n2.Create(1);

    // Create node containers for each link
    NodeContainer n0n1 = NodeContainer(n0.Get(0), n1.Get(0));
    NodeContainer n0n2 = NodeContainer(n0.Get(0), n2.Get(0));

    // Setup point-to-point link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the links
    NetDeviceContainer d0n1 = p2p.Install(n0n1);
    NetDeviceContainer d0n2 = p2p.Install(n0n2);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(n0);
    stack.Install(n1);
    stack.Install(n2);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0n1 = address.Assign(d0n1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0n2 = address.Assign(d0n2);

    // Set up UDP Echo Server on Node 2 (port 9)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(n2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on Node 1, targeting Node 2's IP over the path through n0
    UdpEchoClientHelper echoClient(i0n2.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(n1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // Enable IP forwarding on central node (n0)
    Ptr<Ipv4> ipv4 = n0.Get(0)->GetObject<Ipv4>();
    ipv4->SetAttribute("IpForward", BooleanValue(true));

    // Enable PCAP tracing
    p2p.EnablePcapAll("branch-topology");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}