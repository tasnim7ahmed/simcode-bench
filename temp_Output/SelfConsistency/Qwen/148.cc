#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between node 0 and nodes 1, 2, 3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    Ipv4InterfaceContainer interfaces[3];

    InternetStackHelper stack;
    stack.Install(nodes);

    // Connect node 0 to node 1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    devices[0] = p2p.Install(n0n1);
    // Connect node 0 to node 2
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    devices[1] = p2p.Install(n0n2);
    // Connect node 0 to node 3
    NodeContainer n0n3 = NodeContainer(nodes.Get(0), nodes.Get(3));
    devices[2] = p2p.Install(n0n3);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    address.SetBase("10.1.3.0", "255.255.255.0");
    interfaces[2] = address.Assign(devices[2]);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install UDP echo server on node 0 (port 9)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP echo clients on nodes 1, 2, 3
    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps1 = echoClient.Install(nodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = echoClient.Install(nodes.Get(2));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    ApplicationContainer clientApps3 = echoClient.Install(nodes.Get(3));
    clientApps3.Start(Seconds(4.0));
    clientApps3.Stop(Seconds(10.0));

    // Enable PCAP tracing for all devices
    p2p.EnablePcapAll("branch_topology");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}