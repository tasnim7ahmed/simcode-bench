#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Do not use namespace ns3; // Avoid using global namespace for clarity

int
main(int argc, char* argv[])
{
    // LogComponentEnable("TcpClientServer", LOG_LEVEL_INFO); // Optional: for debugging

    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Point-to-Point link
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 3. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP Addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 5. Create and install TCP Server application
    uint16_t port = 9; // Echo port
    ns3::TcpServerHelper server(port);
    ns3::ApplicationContainer serverApps = server.Install(nodes.Get(1)); // Server on node 1 (second node)
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1s
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at 10s

    // 6. Create and install TCP Client application
    // Client on node 0 (first node), connecting to server on node 1
    ns3::Address serverAddress = ns3::InetSocketAddress(interfaces.GetAddress(1), port);
    ns3::TcpClientHelper client(serverAddress);
    client.SetAttribute("MaxPackets", ns3::UintegerValue(100)); // Send 100 packets (0.1s * 10s = 100 packets roughly)
    client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // Send packets at 0.1s intervals
    client.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1KB packet size

    ns3::ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Client on node 0
    clientApps.Start(ns3::Seconds(0.1)); // Client starts at 0.1s (will attempt connection and retry until server is up)
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10s

    // 7. Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 8. Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}