#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UdpEcho applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Assign names to the first two nodes
    Names::Add("client", nodes.Get(0));
    Names::Add("server", nodes.Get(1));

    // Rename node 2 and node 3
    Names::Add("switch", nodes.Get(2));
    Names::Add("router", nodes.Get(3));

    // Create CSMA helper and configure attributes using Object Names service
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    // Install CSMA devices on all nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Assign IP stack to all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on the node named "server"
    uint16_t echoPort = 9;
    UdpEchoServerHelper server(echoPort);
    ApplicationContainer serverApp = server.Install(Names::Find<Node>("server"));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on the node named "client"
    UdpEchoClientHelper client(interfaces.GetAddress(1), echoPort);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(Names::Find<Node>("client"));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing for all devices
    csma.EnablePcapAll("object-names-csma");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}