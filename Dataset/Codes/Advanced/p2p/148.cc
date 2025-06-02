#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopology4Nodes");

int main(int argc, char *argv[])
{
    LogComponentEnable("BranchTopology4Nodes", LOG_LEVEL_INFO);

    // Create 4 nodes: node 0 as central, nodes 1, 2, and 3 as branches
    NodeContainer nodes;
    nodes.Create(4);

    // Set up point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point links between node 0 and the other nodes (1, 2, and 3)
    NetDeviceContainer device01, device02, device03;
    device01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));  // Link between node 0 and node 1
    device02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));  // Link between node 0 and node 2
    device03 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));  // Link between node 0 and node 3

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(device01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(device02);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces03 = address.Assign(device03);

    // Set up a UDP Echo server on node 0
    UdpEchoServerHelper echoServer(9);  // Port number
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up a UDP Echo client on node 1 to send packets to node 0
    UdpEchoClientHelper echoClient1(interfaces01.GetAddress(0), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    // Set up a UDP Echo client on node 2 to send packets to node 0
    UdpEchoClientHelper echoClient2(interfaces02.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(10.0));

    // Set up a UDP Echo client on node 3 to send packets to node 0
    UdpEchoClientHelper echoClient3(interfaces03.GetAddress(0), 9);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = echoClient3.Install(nodes.Get(3));
    clientApps3.Start(Seconds(3.0));
    clientApps3.Stop(Seconds(10.0));

    // Enable PCAP tracing to capture packet transmissions
    pointToPoint.EnablePcapAll("branch-topology-4-nodes");

    // Run the simulation
    Simulator::Run();

    // End the simulation
    Simulator::Destroy();
    return 0;
}

