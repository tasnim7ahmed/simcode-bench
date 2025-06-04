#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BusTopology4Nodes");

int main(int argc, char *argv[])
{
    LogComponentEnable("BusTopology4Nodes", LOG_LEVEL_INFO);

    // Create 4 nodes: node 0, node 1, node 2, and node 3
    NodeContainer nodes;
    nodes.Create(4);

    // Set up CSMA (bus) channel between nodes
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices on the nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Install the internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up a UDP Echo server on node 3
    UdpEchoServerHelper echoServer(9);  // Port number
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up a UDP Echo client on node 0 to send packets to node 3
    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), 9);  // Address of node 3
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));  // Client on node 0
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing to capture packet transmissions
    csma.EnablePcapAll("bus-topology-4-nodes");

    // Run the simulation
    Simulator::Run();

    // End the simulation
    Simulator::Destroy();
    return 0;
}

