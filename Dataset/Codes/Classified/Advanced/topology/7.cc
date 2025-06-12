#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyExample");

int main(int argc, char* argv[])
{
    // Parse command-line arguments
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Set time resolution to nanoseconds
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes for the ring topology
    NodeContainer ring;
    ring.Create(4);

    // Configure point-to-point links with 5Mbps data rate and 2ms delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create devices for each pair of adjacent nodes in the ring
    NetDeviceContainer device1, device2, device3, device4;
    device1.Add(pointToPoint.Install(ring.Get(0), ring.Get(1)));
    device2.Add(pointToPoint.Install(ring.Get(1), ring.Get(2)));
    device3.Add(pointToPoint.Install(ring.Get(2), ring.Get(3)));
    device4.Add(pointToPoint.Install(ring.Get(3), ring.Get(0)));

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(ring);

    // Assign IP addresses for each point-to-point link
    Ipv4AddressHelper address1, address2, address3, address4;
    address1.SetBase("192.9.39.0", "255.255.255.252");  // Subnet for node 0-1 link
    address2.SetBase("192.9.39.4", "255.255.255.252");  // Subnet for node 1-2 link
    address3.SetBase("192.9.39.8", "255.255.255.252");  // Subnet for node 2-3 link
    address4.SetBase("192.9.39.12", "255.255.255.252"); // Subnet for node 3-0 link

    // Assign IP addresses to each device's interface
    Ipv4InterfaceContainer interface1, interface2, interface3, interface4;
    interface1 = address1.Assign(device1);
    interface2 = address2.Assign(device2);
    interface3 = address3.Assign(device3);
    interface4 = address4.Assign(device4);

    // Create a UDP echo server on each node in sequence
    UdpEchoServerHelper echoServer(9);  // All servers listen on port 9

    // Configure server on node 1 and client on node 0
    ApplicationContainer serverApps10 = echoServer.Install(ring.Get(1));
    serverApps10.Start(Seconds(1.0));
    serverApps10.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient10(interface1.GetAddress(1), 9); // Node 1 as server
    echoClient10.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient10.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient10.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp10 = echoClient10.Install(ring.Get(0)); // Node 0 as client
    clientApp10.Start(Seconds(2.0));
    clientApp10.Stop(Seconds(10.0));

    // Configure server on node 2 and client on node 1
    ApplicationContainer serverApps21 = echoServer.Install(ring.Get(2));
    serverApps21.Start(Seconds(11.0));
    serverApps21.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient21(interface2.GetAddress(1), 9); // Node 2 as server
    echoClient21.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient21.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient21.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp21 = echoClient21.Install(ring.Get(1)); // Node 1 as client
    clientApp21.Start(Seconds(12.0));
    clientApp21.Stop(Seconds(20.0));

    // Configure server on node 3 and client on node 2
    ApplicationContainer serverApps32 = echoServer.Install(ring.Get(3));
    serverApps32.Start(Seconds(21.0));
    serverApps32.Stop(Seconds(30.0));

    UdpEchoClientHelper echoClient32(interface3.GetAddress(1), 9); // Node 3 as server
    echoClient32.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient32.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient32.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp32 = echoClient32.Install(ring.Get(2)); // Node 2 as client
    clientApp32.Start(Seconds(22.0));
    clientApp32.Stop(Seconds(30.0));

    // Configure server on node 0 and client on node 3
    ApplicationContainer serverApps03 = echoServer.Install(ring.Get(0));
    serverApps03.Start(Seconds(31.0));
    serverApps03.Stop(Seconds(40.0));

    UdpEchoClientHelper echoClient03(interface4.GetAddress(1), 9); // Node 0 as server
    echoClient03.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient03.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient03.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp03 = echoClient03.Install(ring.Get(3)); // Node 3 as client
    clientApp03.Start(Seconds(32.0));
    clientApp03.Stop(Seconds(40.0));

    // Enable NetAnim animation output
    AnimationInterface anim("RingTopologyExample.xml");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

