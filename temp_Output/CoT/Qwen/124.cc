#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 4 total (3 VLAN hosts + 1 hub/router)
    NodeContainer nodes;
    nodes.Create(4);

    // Create CSMA channel for shared medium
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100 Mbps)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Install CSMA devices on all nodes
    NetDeviceContainer csmaDevices = csma.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to different subnets (VLANs)
    Ipv4AddressHelper address;

    // Interface containers for each subnet
    Ipv4InterfaceContainer i1i0, i2i0, i3i0, routerInterfaces;

    // Subnet 1: Node 0 and Hub (Node 3)
    address.SetBase("192.168.1.0", "255.255.255.0");
    i1i0 = address.Assign(csmaDevices.Get(0)); // Node 0
    routerInterfaces.Add(csmaDevices.Get(3), 0); // Hub node interface

    // Subnet 2: Node 1 and Hub (Node 3)
    address.SetBase("192.168.2.0", "255.255.255.0");
    i2i0 = address.Assign(csmaDevices.Get(1)); // Node 1
    routerInterfaces.Add(csmaDevices.Get(3), 1); // Hub node interface

    // Subnet 3: Node 2 and Hub (Node 3)
    address.SetBase("192.168.3.0", "255.255.255.0");
    i3i0 = address.Assign(csmaDevices.Get(2)); // Node 2
    routerInterfaces.Add(csmaDevices.Get(3), 2); // Hub node interface

    // Assign the IP addresses to the hub's interfaces
    address.Assign(routerInterfaces);

    // Set up routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing
    csma.EnablePcapAll("vlan_routing_simulation");

    // Setup applications: UDP Echo Server on Node 0
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on Node 1 and Node 2 sending to Node 0
    UdpEchoClientHelper echoClient1(i1i0.GetAddress(0), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(i1i0.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(10.0));

    // Simulation run
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}