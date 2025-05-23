#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main()
{
    // Configure logging for specific components (optional but good for debugging)
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PointToPointNetDevice", ns3::LOG_LEVEL_INFO);

    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Point-to-Point link
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    ns3::NetDeviceContainer devices = p2p.Install(nodes);

    // 3. Install Internet Stack on nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 5. Setup Server (Node 2 - index 1)
    uint16_t port = 50000; // Choose an arbitrary port
    ns3::Address sinkAddress(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(1)); // Node 2 is at index 1
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1s
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at 10s

    // 6. Setup Client (Node 1 - index 0)
    ns3::OnOffHelper clientHelper("ns3::TcpSocketFactory", ns3::Ipv4Address::GetZero()); // Placeholder address, actual remote address set below
    clientHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always on
    clientHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never off
    clientHelper.SetAttribute("DataRate", ns3::StringValue("5Mbps")); // Client sends at 5Mbps
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size 1024 bytes

    // Set the remote address for the client to connect to the server
    ns3::Address remoteAddress(ns3::InetSocketAddress(interfaces.GetAddress(1), port)); // Server's IP is interfaces.GetAddress(1)
    clientHelper.SetAttribute("Remote", ns3::AddressValue(remoteAddress));

    ns3::ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0)); // Node 1 is at index 0
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2s
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10s

    // 7. Set simulation stop time and run
    ns3::Simulator::Stop(ns3::Seconds(10.0)); // Simulation runs for 10 seconds
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}