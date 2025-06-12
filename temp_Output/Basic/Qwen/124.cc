#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HubTrunkVlanRoutingSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    // Create nodes: 4 total (hub + 3 VLANs)
    NodeContainer nodes;
    nodes.Create(4);

    // CSMA helper for hub connection
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100 Mbps)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Connect all nodes to the hub using CSMA
    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stacks
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Hub node is Node 0, acts as router with interfaces on multiple subnets
    // Interface 1: 192.168.1.0/24 - connected to Node 1
    // Interface 2: 10.1.1.0/24   - connected to Node 2
    // Interface 3: 172.16.1.0/24 - connected to Node 3

    Ipv4InterfaceContainer ifHubNode1;
    Ipv4InterfaceContainer ifHubNode2;
    Ipv4InterfaceContainer ifHubNode3;

    // Assign subnet 1 (Node 0 and Node 1)
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer hubIfc1 = address.Assign(devices.Get(0));
    Ipv4InterfaceContainer node1Ifc = address.Assign(devices.Get(1));

    // Assign subnet 2 (Node 0 and Node 2)
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer hubIfc2 = address.Assign(devices.Get(0));
    Ipv4InterfaceContainer node2Ifc = address.Assign(devices.Get(2));

    // Assign subnet 3 (Node 0 and Node 3)
    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer hubIfc3 = address.Assign(devices.Get(0));
    Ipv4InterfaceContainer node3Ifc = address.Assign(devices.Get(3));

    // Set up routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create a UDP echo server on Node 3 (in VLAN 3)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create a UDP echo client on Node 1 (in VLAN 1) sending to Node 3
    UdpEchoClientHelper echoClient(node3Ifc.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet capture
    if (tracing) {
        csma.EnablePcapAll("hub_trunk_vlan_routing", false);
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}