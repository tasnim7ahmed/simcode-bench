#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("TcpClientServerExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Install the internet stack (IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the nodes
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);

    // Create and install the TCP server on Node 0
    uint16_t port = 9; // Port number for TCP connection
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create and install the TCP client on Node 1
    TcpClientHelper tcpClient(interfaces.GetAddress(0), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(1));      // Send 1 packet
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval between packets
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));   // Packet size
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
