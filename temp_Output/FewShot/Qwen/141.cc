#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes forming a ring
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point helper configuration
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point links between adjacent nodes in the ring
    NetDeviceContainer devices[3];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1)); // Link 0-1
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2)); // Link 1-2
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(0)); // Link 2-0

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each link
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[3];

    address.SetBase("10.0.0.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);

    // Set up UDP Echo Server on node 0
    uint16_t port = 9;  // Standard echo port number
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 1 to send to node 0 via node 2
    UdpEchoClientHelper client(interfaces[0].GetAddress(0), port); // node 0's IP from link 0-1
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}