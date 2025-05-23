#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main()
{
    // Create two nodes for the wired network
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Configure Point-to-Point link attributes
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install Point-to-Point devices on the nodes
    ns3::NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // Install the Internet stack (TCP/IP protocols) on the nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup TCP Server (PacketSink) on Node 0
    uint16_t port = 8080; // Server listening port
    ns3::Address sinkAddress(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ns3::ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(0));
    serverApp.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApp.Stop(ns3::Seconds(10.0)); // Server runs until simulation end

    // Setup TCP Client (BulkSendApplication) on Node 1
    ns3::Ipv4Address serverIp = interfaces.GetAddress(0); // Get IP address of Node 0
    ns3::BulkSendHelper clientHelper("ns3::TcpSocketFactory", ns3::InetSocketAddress(serverIp, port));
    clientHelper.SetAttribute("MaxBytes", ns3::UintegerValue(1000000)); // Client sends 1,000,000 bytes
    ns3::ApplicationContainer clientApp = clientHelper.Install(nodes.Get(1));
    clientApp.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds
    clientApp.Stop(ns3::Seconds(9.0));  // Client stops at 9 seconds to ensure data transfer completes before simulation end

    // Set the total simulation time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}