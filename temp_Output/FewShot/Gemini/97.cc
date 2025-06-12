#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Assign names to nodes
    Names::Add("client", nodes.Get(0));
    Names::Add("server", nodes.Get(1));
    Names::Add("node2", nodes.Get(2));
    Names::Add("node3", nodes.Get(3));

    // Rename a node
    Names::Add("renamed_node", nodes.Get(2));
    Names::Remove(nodes.Get(2));

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    // Install CSMA devices and interfaces
    NetDeviceContainer devices = csma.Install(nodes);

    // Assign names to CSMA devices
    Names::Add("client_csma", devices.Get(0));
    Names::Add("server_csma", devices.Get(1));
    Names::Add("node2_csma", devices.Get(2));
    Names::Add("node3_csma", devices.Get(3));

    // Configure CSMA attributes using Object Names
    Config::Set("/Names/client_csma/Mtu", UintegerValue(1400));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP echo server on the "server" node
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(Names::Find<Node>("server"));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP echo client on the "client" node
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(Names::Find<Node>("client"));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet tracing
    csma.EnablePcapAll("object-names-csma");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}