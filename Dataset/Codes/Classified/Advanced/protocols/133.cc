#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/distance-vector-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SplitHorizonSimulation");

int main(int argc, char *argv[])
{
    // Enable logging for the components involved
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links
    NetDeviceContainer ab = p2p.Install(nodes.Get(0), nodes.Get(1)); // Link A-B
    NetDeviceContainer bc = p2p.Install(nodes.Get(1), nodes.Get(2)); // Link B-C

    // Install Internet stack (with Distance Vector and Split Horizon)
    InternetStackHelper internet;
    DistanceVectorHelper dvRouting;
    dvRouting.Set("SplitHorizon", BooleanValue(true)); // Enabling Split Horizon
    internet.SetRoutingHelper(dvRouting);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer abIf = ipv4.Assign(ab); // A-B

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bcIf = ipv4.Assign(bc); // B-C

    // Create UDP echo server on node C
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2)); // C as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Create UDP echo client on node A
    UdpEchoClientHelper echoClient(bcIf.GetAddress(1), 9); // Sending to C
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // A as client
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing to observe packet flow
    p2p.EnablePcapAll("split-horizon");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

