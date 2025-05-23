#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-echo-helper.h"

int
main(int argc, char* argv[])
{
    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Point-to-Point link
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // 3. Install NetDevices on nodes
    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 4. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Install(devices);

    // 6. Set up UDP Echo Server on Node 2 (index 1)
    uint16_t port = 9; // Echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // 7. Set up UDP Echo Client on Node 1 (index 0)
    // Node 1 sends packets to Node 2's IP address
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(1); // IP address of Node 2
    ns3::UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(100)); // Send 100 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // One packet per second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes packet size

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    // 8. Set simulation run time
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}