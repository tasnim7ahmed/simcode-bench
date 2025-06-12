#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetUdpExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("ManetUdpExample", LOG_LEVEL_INFO);

    // Create two nodes: one for the server and one for the client
    NodeContainer nodes;
    nodes.Create(2);

    // Set up mobility model for the nodes (to simulate movement)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Time", TimeValue(Seconds(2.0)),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1|Max=5]"));
    mobility.Install(nodes);

    // Install the Internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Set up the point-to-point link (no physical link here, just logical for communication)
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP server on node 1 (server side)
    UdpServerHelper udpServer(9); // Port 9
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Install UDP client on node 0 (client side)
    UdpClientHelper udpClient(interfaces.GetAddress(1), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));    // Send 100 packets
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Send packets every 0.1 seconds
    udpClient.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes packets
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
