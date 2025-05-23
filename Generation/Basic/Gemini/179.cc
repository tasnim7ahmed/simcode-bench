#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    // 1. Set TCP NewReno congestion control algorithm
    ns3::Config::SetDefault("ns3::TcpL4Protocol::CongestionControlAlgorithm", ns3::StringValue("ns3::TcpNewReno"));
    // Set initial congestion window to 1
    ns3::Config::SetDefault("ns3::TcpSocket::InitialCwnd", ns3::UintegerValue(1));
    // Set TCP receive buffer size
    ns3::Config::SetDefault("ns3::TcpL4Protocol::RxBufferSize", ns3::UintegerValue(131072)); // 128KB

    // 2. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 3. Create Point-to-Point link attributes
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("10ms"));

    // Install devices on nodes
    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 4. Install Internet Stack on nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses to devices
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Create PacketSink application (receiver/server on Node 1)
    uint16_t sinkPort = 9; // Common port for discard/echo, can be any valid port
    ns3::Address sinkAddress(ns3::InetSocketAddress(interfaces.GetAddress(1), sinkPort)); // Node 1's address

    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort));
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1)); // Install on Node 1
    sinkApps.Start(ns3::Seconds(0.0));   // Start PacketSink at the beginning of simulation
    sinkApps.Stop(ns3::Seconds(10.0));   // Stop PacketSink at the end of simulation

    // 7. Create BulkSend application (sender/client on Node 0)
    ns3::BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", ns3::Address());
    bulkSendHelper.SetAttribute("Remote", ns3::AddressValue(sinkAddress));
    bulkSendHelper.SetAttribute("MaxBytes", ns3::UintegerValue(0)); // Send unlimited bytes
    bulkSendHelper.SetAttribute("SendSize", ns3::UintegerValue(1024)); // Send 1024 bytes per packet

    ns3::ApplicationContainer clientApps = bulkSendHelper.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(ns3::Seconds(1.0)); // Start BulkSend 1 second into simulation
    clientApps.Stop(ns3::Seconds(9.0));  // Stop BulkSend 1 second before simulation ends

    // 8. Enable NetAnim visualization
    ns3::AnimationInterface anim("p2p-bulk-send.xml");
    anim.SetConstantPosition(nodes.Get(0), 20.0, 50.0); // Position Node 0
    anim.SetConstantPosition(nodes.Get(1), 80.0, 50.0); // Position Node 1

    // 9. Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 10. Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}