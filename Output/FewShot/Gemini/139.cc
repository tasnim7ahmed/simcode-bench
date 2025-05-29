#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6);

    // Create point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices34 = pointToPoint.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer devices45 = pointToPoint.Install(nodes.Get(4), nodes.Get(5));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces34 = address.Assign(devices34);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces45 = address.Assign(devices45);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP Echo Server on node 5
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(5));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces45.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}