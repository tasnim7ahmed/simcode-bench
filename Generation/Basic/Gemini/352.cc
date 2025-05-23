#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // 1. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Create Point-to-Point link
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer devices;
    devices = p2pHelper.Install(nodes);

    // 3. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0"); // Subnet 192.168.1.0/24

    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);
    // Node 0 (client) will have 192.168.1.1
    // Node 1 (server) will have 192.168.1.2

    // 5. Configure TCP Server (PacketSink)
    uint16_t serverPort = 9; // Arbitrary port for the server
    ns3::Address sinkAddress(ns3::InetSocketAddress(interfaces.GetAddress(1), serverPort)); // Server's IP is 192.168.1.2

    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1)); // Install on node 1 (server)
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0)); // Server runs for the entire simulation duration

    // 6. Configure TCP Client (OnOffApplication)
    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory", ns3::Address()); // Placeholder address
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Always On
    onoffHelper.SetAttribute("DataRate", ns3::StringValue("5Mbps"));
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes

    // Set the remote address for the client application to point to the server
    ns3::AddressValue remoteAddress(ns3::InetSocketAddress(interfaces.GetAddress(1), serverPort)); // Connect to server's IP and port
    onoffHelper.SetAttribute("Remote", remoteAddress);

    ns3::ApplicationContainer clientApps = onoffHelper.Install(nodes.Get(0)); // Install on node 0 (client)
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at simulation end time

    // 7. Set simulation end time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 8. Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}