#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiredTcpExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("WiredTcpExample", LOG_LEVEL_INFO);

    // Create two nodes representing the computers
    NodeContainer nodes;
    nodes.Create(2);

    // Set up the point-to-point connection between the two computers
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server on the second node (Computer 2)
    TcpServerHelper tcpServer(9); // Listening on port 9
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on the first node (Computer 1)
    TcpClientHelper tcpClient(interfaces.GetAddress(1), 9);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(10));     // Send 10 packets
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
