#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("TcpClientServerExample", LOG_LEVEL_INFO);

    // Create nodes for the network
    NodeContainer nodes;
    nodes.Create(2); // Two nodes: Client and Server

    // Install the internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Set up IP addresses for the nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes.Get(0), nodes.Get(1));

    // Set up the TCP server application on Node 1 (Server)
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    TcpServerHelper tcpServer(port);                                  // TCP port 8080
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Install on Node 1 (Server)
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the TCP client application on Node 0 (Client)
    TcpClientHelper tcpClient(serverAddress);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(10));          // Number of packets to send
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));      // Interval between packets
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));        // Packet size in bytes
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Install on Node 0 (Client)
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
