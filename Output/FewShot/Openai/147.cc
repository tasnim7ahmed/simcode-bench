#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for UdpEcho applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes: n0 (central), n1, n2
    NodeContainer n0n1;
    n0n1.Add(CreateObject<Node>());
    n0n1.Add(CreateObject<Node>());

    NodeContainer n0n2;
    n0n2.Add(n0n1.Get(0)); // n0
    n0n2.Add(CreateObject<Node>()); // n2

    // Name nodes for clarity
    Ptr<Node> n0 = n0n1.Get(0);
    Ptr<Node> n1 = n0n1.Get(1);
    Ptr<Node> n2 = n0n2.Get(1);

    // Install point-to-point connections
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(n0);
    stack.Install(n1);
    stack.Install(n2);

    // Assign IP addresses
    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address01.Assign(d0d1);

    Ipv4AddressHelper address02;
    address02.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address02.Assign(d0d2);

    // Set up UDP Echo Servers on node 1 and node 2
    uint16_t port1 = 9001;
    uint16_t port2 = 9002;

    UdpEchoServerHelper echoServer1(port1);
    ApplicationContainer serverApp1 = echoServer1.Install(n1);
    serverApp1.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer2(port2);
    ApplicationContainer serverApp2 = echoServer2.Install(n2);
    serverApp2.Start(Seconds(1.0));
    serverApp2.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on node 0 for both servers
    UdpEchoClientHelper echoClient1(i0i1.GetAddress(1), port1);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp1 = echoClient1.Install(n0);
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(i0i2.GetAddress(1), port2);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp2 = echoClient2.Install(n0);
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

    // Enable PCAP tracing on point-to-point devices
    p2p.EnablePcapAll("branch-topology");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}