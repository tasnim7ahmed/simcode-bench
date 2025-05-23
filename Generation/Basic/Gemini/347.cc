#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant components
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("TcpSocketBase", ns3::LOG_LEVEL_INFO); // General TCP socket info

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // Node 0 and Node 1

    // Create a Point-to-Point link between the nodes
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("1ms"));

    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack on both nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the network devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices); // interfaces.GetAddress(0) for Node 0, interfaces.GetAddress(1) for Node 1

    // Set up the TCP Server (PacketSink) on Node 0
    uint16_t port = 9; // Common port for testing (e.g., Echo)

    // The server listens on the IP address of Node 0
    ns3::Address serverAddress = ns3::InetSocketAddress(interfaces.GetAddress(0), port);

    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));  // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at 10 seconds

    // Set up the TCP Client (OnOffApplication) on Node 1
    ns3::OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress); // Client connects to serverAddress
    
    // Configure client to send one 1024-byte packet with a 1-second interval
    // This is achieved by setting PacketSize to 1024 bytes and DataRate to 1024 bytes/sec (8192 bps).
    // With OnTime=1.0 and OffTime=0.0, the application attempts to send 1024 bytes each second it's active.
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // No off time
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes per packet
    onoff.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate("8192bps"))); // 1024 bytes/sec (1024 * 8 bits/byte)

    ns3::ApplicationContainer clientApps = onoff.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(2.0));  // Client starts at 2 seconds
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10 seconds

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}