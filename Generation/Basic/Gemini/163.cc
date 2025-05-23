#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

// Do not use a namespace to avoid potential issues with user's ns-3 environment
// using namespace ns3;

int main(int argc, char* argv[])
{
    // Set default TCP type to NewReno
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpNewReno"));

    // Allow logging for some components (optional, can be removed)
    // ns3::LogComponentEnable("TcpClient", ns3::LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);

    // Set time resolution to nanoseconds (default is usually fine)
    ns3::Time::SetResolution(ns3::Time::NS);

    // Create two nodes: client and server
    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::Ptr<ns3::Node> clientNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> serverNode = nodes.Get(1);

    // Configure Point-to-Point link
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("1ms"));

    // Install devices on nodes
    ns3::NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack on nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    ns3::Ipv4Address serverAddress = interfaces.GetAddress(1); // Server is node 1

    // Server application: PacketSink
    uint16_t port = 9; // Echo port
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::Any, port));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(serverNode);
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Client application: TcpClient
    // Set MaxBytes to ensure a single data transfer after which the connection terminates.
    // Here, 1024 bytes (1KB) will be sent.
    ns3::TcpClientHelper tcpClientHelper(serverAddress, port);
    tcpClientHelper.SetAttribute("MaxBytes", ns3::UintegerValue(1024)); // Send 1KB and then terminate
    // Note: If MaxBytes is set, Interval and other send-related attributes are less relevant for termination.
    // The client will send the specified bytes and close its socket.
    ns3::ApplicationContainer clientApps = tcpClientHelper.Install(clientNode);
    clientApps.Start(ns3::Seconds(1.0)); // Start client after server is ready
    clientApps.Stop(ns3::Seconds(10.0));

    // Enable NetAnim visualization
    ns3::NetAnimHelper netanim;
    netanim.SetOutputFileName("tcp-non-persistent.xml");
    netanim.EnablePacketTracking(); // Track packets for visualization
    netanim.EnableIpv4RouteTracking(); // Track IPv4 routing (optional)

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}