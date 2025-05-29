#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/object-names.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Naming nodes
    Names::Add("client", nodes.Get(0));
    Names::Add("n1", nodes.Get(1));
    Names::Add("n2", nodes.Get(2));
    Names::Add("server", nodes.Get(3));

    // CSMA LAN
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Naming devices
    Names::Add("client/csmaDev", devices.Get(0));
    Names::Add("n1/csmaDev", devices.Get(1));
    Names::Add("n2/csmaDev", devices.Get(2));
    Names::Add("server/csmaDev", devices.Get(3));

    // Configure attribute via object names
    Config::Set("/Names/client/csmaDev/TxQueue/MaxPackets", UintegerValue(100));
    Config::Set("/Names/server/csmaDev/TxQueue/MaxPackets", UintegerValue(50));

    // Mix object names and node attributes
    Config::Set("/Names/client/$ns3::CsmaNetDevice/TxQueue/MaxPackets", UintegerValue(75));

    // Rename node 'n1' to 'switch'
    Ptr<Node> n1Ptr = Names::Find<Node>("n1");
    Names::Rename("n1", "switch");

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on "server"
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    Ptr<Node> serverNode = Names::Find<Node>("server");
    ApplicationContainer serverApp = server.Install(serverNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Echo Client on "client"
    UdpEchoClientHelper client(interfaces.GetAddress(3), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    Ptr<Node> clientNode = Names::Find<Node>("client");
    ApplicationContainer clientApp = client.Install(clientNode);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable packet tracing and PCAP
    csma.EnablePcapAll("csma-names", true);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}