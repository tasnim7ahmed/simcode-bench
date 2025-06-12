#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 4 in total, one as hub (router), three as VLAN subnets
    NodeContainer allNodes;
    allNodes.Create(4);

    NodeContainer hub = NodeContainer(allNodes.Get(0));
    NodeContainer vlan1 = NodeContainer(allNodes.Get(1));
    NodeContainer vlan2 = NodeContainer(allNodes.Get(2));
    NodeContainer vlan3 = NodeContainer(allNodes.Get(3));

    // CSMA helper for each subnet
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install Internet stack on all nodes
    InternetStackHelper stack;

    // Hub node acts as router, needs IP forwarding enabled
    stack.Install(hub);
    stack.Install(vlan1);
    stack.Install(vlan2);
    stack.Install(vlan3);

    // Assign different subnets to each VLAN
    Ipv4AddressHelper address;

    // Interfaces container to hold interface addresses
    NetDeviceContainer devicesHubVlan1, devicesHubVlan2, devicesHubVlan3;
    NetDeviceContainer devicesVlan1Node, devicesVlan2Node, devicesVlan3Node;

    // Subnet 1: 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    devicesHubVlan1 = csma.Install(NodeContainer(hub, vlan1));
    Ipv4InterfaceContainer interfacesVlan1 = address.Assign(devicesHubVlan1);

    // Subnet 2: 192.168.2.0/24
    address.SetBase("192.168.2.0", "255.255.255.0");
    devicesHubVlan2 = csma.Install(NodeContainer(hub, vlan2));
    Ipv4InterfaceContainer interfacesVlan2 = address.Assign(devicesHubVlan2);

    // Subnet 3: 192.168.3.0/24
    address.SetBase("192.168.3.0", "255.255.255.0");
    devicesHubVlan3 = csma.Install(NodeContainer(hub, vlan3));
    Ipv4InterfaceContainer interfacesVlan3 = address.Assign(devicesHubVlan3);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing on all CSMA devices
    csma.EnablePcapAll("hub_vlan_network");

    // Optional: Add echo server and client applications to test inter-subnet communication
    uint16_t port = 9;

    // Setup UDP Echo Server on VLAN3 node
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vlan3);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup UDP Echo Client on VLAN1 node sending to VLAN3 node
    Ipv4Address serverIp = interfacesVlan3.GetAddress(1); // VLAN3 node's IP
    UdpEchoClientHelper echoClient(serverIp, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(vlan1);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}