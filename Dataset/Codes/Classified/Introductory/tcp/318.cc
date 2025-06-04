#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTcpExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("SimpleTcpExample", LOG_LEVEL_INFO);

    // Create two nodes: one for the TCP client, one for the TCP server
    NodeContainer nodes;
    nodes.Create(2);

    // Install internet stack (IP, TCP, UDP)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Set up TCP server on the second node
    uint16_t port = 8080;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on the first node
    InetSocketAddress remoteAddress = InetSocketAddress(Ipv4Address("10.1.1.2"), port);
    TcpClientHelper tcpClient(remoteAddress);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(10));
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Assign IP addresses to the nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes.Get(0)->GetDevice(0));
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(nodes.Get(1)->GetDevice(0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
