#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LineTopology3Nodes");

int main(int argc, char *argv[])
{
    LogComponentEnable("LineTopology3Nodes", LOG_LEVEL_INFO);

    // Create 3 nodes: node 0, node 1, and node 2
    NodeContainer nodes;
    nodes.Create(3);

    // Set up point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point links between node 0 to node 1 and node 1 to node 2
    NetDeviceContainer device01, device12;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));  // Link between node 1 and node 2

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(device01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(device12);

    // Set up a UDP Echo server on node 2
    UdpEchoServerHelper echoServer(9);  // Port number
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up a UDP Echo client on node 0 to send packets to node 2
    UdpEchoClientHelper echoClient(interfaces12.GetAddress(1), 9);  // Address of node 2
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));  // Client on node 0
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing to capture packet transmissions
    pointToPoint.EnablePcapAll("line-topology-3-nodes");

    // Run the simulation
    Simulator::Run();

    // End the simulation
    Simulator::Destroy();
    return 0;
}

