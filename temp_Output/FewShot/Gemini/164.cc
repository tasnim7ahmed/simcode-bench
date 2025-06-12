#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ospf-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create Point-to-Point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices3 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
    NetDeviceContainer devices4 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet Stack
    InternetStackHelper internet;
    OspfHelper ospf;
    internet.SetRoutingHelper(ospf);
    internet.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces4 = address.Assign(devices4);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create Applications (UDP Echo Client and Server)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces2.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable NetAnim
    AnimationInterface anim("ospf-animation.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 50, 50);
    anim.SetConstantPosition(nodes.Get(2), 90, 10);
    anim.SetConstantPosition(nodes.Get(3), 50, 10);

    // Run Simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}