#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Point-to-Point link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 3. Install devices on nodes
    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // 4. Install Internet stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Setup UDP Echo Server on the second node (node 1)
    uint16_t port = 9; // Echo port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // 7. Setup UDP Echo Client on the first node (node 0)
    // Client sends to the server's IP address (interfaces.GetAddress(1))
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100)); // Send 100 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send every 1 second
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0)); // Start client after server is up
    clientApps.Stop(Seconds(10.0));

    // 8. Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // 9. Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}