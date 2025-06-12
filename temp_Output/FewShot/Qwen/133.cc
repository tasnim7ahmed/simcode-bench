#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging for DV routing to observe routing table updates
    LogComponentEnable("DvRoutingProtocol", LOG_LEVEL_INFO);

    // Create three nodes: A (0), B (1), C (2)
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stack
    InternetStackHelper stack;
    DvRoutingHelper dv;
    stack.SetRoutingHelper(dv);  // Use Distance Vector routing
    stack.Install(nodes);

    // Connect Node A to Node B
    NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Connect Node B to Node C
    NetDeviceContainer devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = ip.Assign(devicesAB);

    ip.NewNetwork();
    Ipv4InterfaceContainer interfacesBC = ip.Assign(devicesBC);

    // Enable routing tables output to verify Split Horizon behavior
    dv.PrintRoutingTableAllAt(Seconds(10.0));

    // UDP Echo Server on Node C (IP: interface BC index 1)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // UDP Echo Client on Node A (send to Node C's IP)
    UdpEchoClientHelper echoClient(interfacesBC.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}