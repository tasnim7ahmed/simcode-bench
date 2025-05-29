#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 10.0;

    // Create Nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Create point-to-point links
    NodeContainer ab(nodeA, nodeB);
    NodeContainer bc(nodeB, nodeC);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB = p2p.Install(ab);
    NetDeviceContainer devBC = p2p.Install(bc);

    // CsmaNetDevices for A and C (for /32 endpoints)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer csmaA, csmaC;
    csmaA = csma.Install(NodeContainer(nodeA));
    csmaC = csma.Install(NodeContainer(nodeC));

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // IP assignment
    // p2p: AB - x.x.x.0/30, BC - y.y.y.0/30
    // We'll pick 10.0.0.0/30 and 10.0.0.4/30 for quick IPs
    Ipv4AddressHelper ipv4; 
    // AB: 10.0.0.0/30
    ipv4.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);

    // BC: 10.0.0.4/30
    ipv4.SetBase("10.0.0.4", "255.255.255.252");
    Ipv4InterfaceContainer ifBC = ipv4.Assign(devBC);

    // Csma endpoints
    // NodeA: 172.16.1.1/32
    ipv4.SetBase("172.16.1.1", "255.255.255.255");
    Ipv4InterfaceContainer ifAhost = ipv4.Assign(csmaA);

    // NodeC: 192.168.1.1/32
    ipv4.SetBase("192.168.1.1", "255.255.255.255");
    Ipv4InterfaceContainer ifChost = ipv4.Assign(csmaC);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP application: A (sender) → C (sink)
    uint16_t port = 5000;
    // Sink on C
    Address sinkAddress(InetSocketAddress(ifChost.GetAddress(0), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodeC);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // Constant rate sender on A
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    ApplicationContainer app = onoff.Install(nodeA);

    // Enable PCAP tracing
    p2p.EnablePcapAll("three-router-p2p");
    csma.EnablePcap("three-router-csmaA", csmaA.Get(0), true);
    csma.EnablePcap("three-router-csmaC", csmaC.Get(0), true);

    // Enable Ascii tracing if desired
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router-p2p.tr"));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}