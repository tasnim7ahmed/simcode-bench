#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int
main(int argc, char* argv[])
{
    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Point-to-Point link characteristics
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // 3. Install Point-to-Point devices on nodes and link them
    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 4. Install standard Internet protocols (TCP/IP stack) on nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IP addresses to the devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Configure UDP Echo Server on Node 0
    uint16_t port = 9; // UDP Echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at 10 seconds

    // 7. Configure UDP Echo Client on Node 1
    ns3::Ptr<ns3::Node> clientNode = nodes.Get(1);
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(0); // Server's IP address

    ns3::UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(1)); // Send a single packet
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024-byte message

    ns3::ApplicationContainer clientApps = echoClient.Install(clientNode);
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10 seconds (before simulation ends)

    // 8. Enable PCAP capture for the point-to-point devices
    pointToPoint.EnablePcapAll("p2p-echo");

    // 9. Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 10. Run the simulation
    ns3::Simulator::Run();

    // 11. Clean up
    ns3::Simulator::Destroy();

    return 0;
}