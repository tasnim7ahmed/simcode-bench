#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/names.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Assign human-readable names to nodes
    Names::Add("client", nodes.Get(0));
    Names::Add("server", nodes.Get(1));
    Names::Add("routerA", nodes.Get(2));
    Names::Add("routerB", nodes.Get(3));

    // Setup CSMA LAN
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices
    NetDeviceContainer devices = csma.Install(nodes);

    // Assign device names and add to Object Names
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<NetDevice> dev = devices.Get(i);
        std::ostringstream oss;
        if (i == 0)
            oss << "csma-client";
        else if (i == 1)
            oss << "csma-server";
        else if (i == 2)
            oss << "csma-routerA";
        else
            oss << "csma-routerB";
        Names::Add(oss.str(), dev);
    }

    // Demonstrate renaming in the Object Names service
    Ptr<Node> oldServer = Names::Find<Node>("server");
    Names::Rename("server", "mainServer");
    Names::Add("server", oldServer); // Re-add as "server" (now both "mainServer" and "server" map to the node)

    // Configure attributes of CSMA devices using Object Names
    Config::Set("/Names/csma-client/TxQueue/MaxSize", StringValue("500p"));

    // Mix object names and node attributes in the configuration system
    Config::Set("/Names/mainServer/$ns3::Ipv4L3Protocol/TxQueue/MaxSize", StringValue("300p"));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Enable pcap tracing
    csma.EnablePcapAll("object-names-csma", true);

    // UDP Echo server on the node with name "server"
    Ptr<Node> serverNode = Names::Find<Node>("server");
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(serverNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // UDP Echo client on the node with name "client"
    Ptr<Node> clientNode = Names::Find<Node>("client");
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(128));
    ApplicationContainer clientApps = echoClient.Install(clientNode);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}