#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link A <-> B with subnet x.x.x.0/30
    NetDeviceContainer devAB;
    devAB = p2p.Install(nodeA, nodeB);
    Ipv4AddressHelper addrAB;
    addrAB.SetBase("x.x.x.0", "255.255.255.252");
    Ipv4InterfaceContainer ifAB = addrAB.Assign(devAB);

    // Link B <-> C with subnet y.y.y.0/30
    NetDeviceContainer devBC;
    devBC = p2p.Install(nodeB, nodeC);
    Ipv4AddressHelper addrBC;
    addrBC.SetBase("y.y.y.0", "255.255.255.252");
    Ipv4InterfaceContainer ifBC = addrBC.Assign(devBC);

    // Install InternetStack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign /32 IP addresses to node A and node C
    Ipv4AddressHelper addrA;
    addrA.SetBase("172.16.1.1", "255.255.255.255");
    Ipv4InterfaceContainer csmaIfA;
    CsmaHelper csmaA;
    csmaA.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaA.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560));
    NetDeviceContainer csmaDevA = csmaA.Install(nodeA);
    csmaIfA = addrA.Assign(csmaDevA);

    Ipv4AddressHelper addrC;
    addrC.SetBase("192.168.1.1", "255.255.255.255");
    Ipv4InterfaceContainer csmaIfC;
    CsmaHelper csmaC;
    csmaC.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaC.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560));
    NetDeviceContainer csmaDevC = csmaC.Install(nodeC);
    csmaIfC = addrC.Assign(csmaDevC);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP Echo Server (Packet Sink) on node C
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodeC);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client (Sender) on node A
    UdpEchoClientHelper echoClient(csmaIfC.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(nodeA);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-udp.tr"));
    csmaA.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-csmaA.tr"));
    csmaC.EnableAsciiAll(asciiTraceHelper.CreateFileStream("three-router-csmaC.tr"));

    // Enable packet capture
    p2p.EnablePcapAll("three-router-pcap");
    csmaA.EnablePcapAll("three-router-csmaA-pcap");
    csmaC.EnablePcapAll("three-router-csmaC-pcap");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}