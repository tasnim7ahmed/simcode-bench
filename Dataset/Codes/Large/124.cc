#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrunkPortSimulation");

int main(int argc, char *argv[])
{
    // Step 1: Create nodes
    NodeContainer hubNode; // Central hub node
    hubNode.Create(1);

    NodeContainer clientNodes;
    clientNodes.Create(3); // Three client nodes representing VLANs

    // Step 2: Create a CSMA network (shared medium like a hub)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices to the nodes
    NodeContainer allNodes = NodeContainer(hubNode, clientNodes);
    NetDeviceContainer devices = csma.Install(allNodes);

    // Step 3: Install the internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Step 4: Assign IP addresses to different VLANs (subnets)
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan1Interfaces = ipv4.Assign(devices.Get(0)); // Hub side
    vlan1Interfaces.Add(ipv4.Assign(devices.Get(1)));                     // Client 1

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan2Interfaces = ipv4.Assign(devices.Get(2)); // Client 2

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan3Interfaces = ipv4.Assign(devices.Get(3)); // Client 3

    // Step 5: Set up routing between the VLANs
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Step 6: Create applications to simulate communication between VLANs
    // UDP Echo Server on node 1 (Client 1 in VLAN 1)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(clientNodes.Get(0)); // Client 1
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on node 2 (Client 2 in VLAN 2), sending to Client 1
    UdpEchoClientHelper echoClient(vlan1Interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(clientNodes.Get(1)); // Client 2
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Step 7: Enable packet tracing (pcap tracing)
    csma.EnablePcap("trunk-port-simulation", devices);

    // Step 8: Run the simulation
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

