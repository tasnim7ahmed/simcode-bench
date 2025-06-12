#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UDP echo applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Configure CSMA helper with 100Mbps data rate
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses in the 10.1.1.0/24 range
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on node 3 (last node)
    uint16_t port = 9;  // Well-known echo port
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on nodes 0, 1, and 2
    for (uint32_t i = 0; i < 3; ++i) {
        UdpEchoClientHelper client(interfaces.GetAddress(3), port); // Target server IP
        client.SetAttribute("MaxPackets", UintegerValue(9));        // 9 packets over 9 seconds
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.1)); // Staggered start times to avoid collisions
        clientApp.Stop(Seconds(10.0));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}