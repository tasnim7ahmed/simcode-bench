#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/distance-vector-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoNodeLoopDVExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: A, B
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point link between A and B
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ab = p2p.Install(nodes);

    // Install the Internet stack on nodes
    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;  // Use DV routing (simulating RIP)
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIf = ipv4.Assign(ab);

    // Create UDP echo server on node B
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1)); // B as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Create UDP echo client on node A
    UdpEchoClientHelper echoClient(abIf.GetAddress(1), 9);  // Sending to B
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // A as client
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Simulate link failure between A and B at t = 5s
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(1));
    Simulator::Schedule(Seconds(5.0), &NetDevice::SetLinkDown, ab.Get(0));

    // Re-enable link after 8s to simulate routing loop
    Simulator::Schedule(Seconds(8.0), &NetDevice::SetLinkUp, ab.Get(1));
    Simulator::Schedule(Seconds(8.0), &NetDevice::SetLinkUp, ab.Get(0));

    // Enable pcap tracing
    p2p.EnablePcapAll("two-node-loop");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

