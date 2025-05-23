#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // Node 0 and Node 1

    // Configure Point-to-Point link characteristics
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install devices on nodes
    ns3::NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // Install Internet stack on nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses from the 192.168.1.x subnet
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure PacketSink application (receiver) on Node 1
    uint16_t sinkPort = 8080;
    ns3::Address sinkAddress(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort));
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1)); // Install on Node 1
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(10.5)); // Run slightly longer than the sender

    // Configure BulkSendApplication (sender) on Node 0
    ns3::Ipv4Address remoteIp = interfaces.GetAddress(1); // IP address of Node 1
    ns3::BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", ns3::Address(ns3::InetSocketAddress(remoteIp, sinkPort)));
    bulkSendHelper.SetAttribute("MaxBytes", ns3::UintegerValue(0)); // Unlimited data
    ns3::ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0)); // Install on Node 0
    sourceApps.Start(ns3::Seconds(1.0)); // Start at 1 second
    sourceApps.Stop(ns3::Seconds(10.0)); // Stop at 10 seconds

    // Set the overall simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.5));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}