#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("TcpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure point-to-point links with 1Mbps data rate and 2ms delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect Node 0 to Node 2
    NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));

    // Connect Node 1 to Node 2
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    // Set up TCP Echo Server on Node 2
    uint16_t port = 9;  // TCP port number
    TcpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP Echo Client on Node 0 (starts at 2 seconds)
    TcpEchoClientHelper echoClient0(interfaces02.GetAddress(1), port);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp0 = echoClient0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(10.0));

    // Set up TCP Echo Client on Node 1 (starts at 3 seconds)
    TcpEchoClientHelper echoClient1(interfaces12.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(3.0));
    clientApp1.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}