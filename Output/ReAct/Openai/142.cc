#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopology3Nodes");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3); // Node 0: center, Node 1 & 2: peripherals

    // Create two point-to-point links: 0-1 and 0-2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Link 0 <-> 1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Link 0 <-> 2
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    // Set up UDP server on node 2
    uint16_t udpServerPort = 9;
    UdpEchoServerHelper echoServer(udpServerPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client on node 1 (sending packets to node 2)
    UdpEchoClientHelper echoClient(i0i2.GetAddress(1), udpServerPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("star-topology");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}