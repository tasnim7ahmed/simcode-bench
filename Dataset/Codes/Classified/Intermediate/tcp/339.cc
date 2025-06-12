#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("TcpClientServerExample", LOG_LEVEL_INFO);

    // Create two nodes: one for the server and one for the client
    NodeContainer nodes;
    nodes.Create(2);

    // Set up the point-to-point link between the client and the server
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the nodes
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the TCP server on node 1 (server side)
    uint16_t port = 9; // Port number
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the TCP client on node 0 (client side)
    TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(100));    // Send 100 packets
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 seconds
    tcpClient.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes packets
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
