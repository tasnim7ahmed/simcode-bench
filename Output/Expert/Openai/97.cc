#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/applications-module.h"
#include "ns3/object-names.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable checksum for proper UDP application
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Assign human-readable names to nodes
    Names::Add("client", nodes.Get(0));
    Names::Add("node1", nodes.Get(1));
    Names::Add("node2", nodes.Get(2));
    Names::Add("server", nodes.Get(3));

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install NetDevices
    NetDeviceContainer devices = csma.Install(nodes);

    // Assign names to NetDevices
    Names::Add("client/csmaDev", devices.Get(0));
    Names::Add("node1/csmaDev", devices.Get(1));
    Names::Add("node2/csmaDev", devices.Get(2));
    Names::Add("server/csmaDev", devices.Get(3));

    // Demonstrate renaming nodes
    Names::Rename("node1", "relay1");
    Names::Rename("node2", "relay2");

    // Configure device attributes using the name service
    Config::Set("/Names/client/csmaDev/TxQueue/MaxPackets", UintegerValue(200));
    Config::Set("/Names/server/csmaDev/TxQueue/MaxPackets", UintegerValue(50));

    // Mix object names and node attributes: Set MAC address via Config
    Config::Set("/Names/relay1/csmaDev/Address", Mac48AddressValue(Mac48Address::Allocate()));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP Echo server on server node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(Names::Find<Node>("server"));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo client on client node
    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(Names::Find<Node>("client"));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable per-device pcap tracing using Object Names
    csma.EnablePcap("/tmp/client", Names::Find<NetDevice>("client/csmaDev"), true, true);
    csma.EnablePcap("/tmp/server", Names::Find<NetDevice>("server/csmaDev"), true, true);
    csma.EnablePcap("/tmp/relay1", Names::Find<NetDevice>("relay1/csmaDev"), true, true);
    csma.EnablePcap("/tmp/relay2", Names::Find<NetDevice>("relay2/csmaDev"), true, true);

    // Run the simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}