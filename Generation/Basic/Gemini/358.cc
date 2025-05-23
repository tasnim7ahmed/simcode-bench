#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

int main()
{
    // Create four nodes
    ns3::NodeContainer nodes;
    nodes.Create(4);

    // Set up CSMA network characteristics
    ns3::CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", ns3::TimeValue(ns3::MilliSeconds(2)));

    // Install CSMA devices on nodes
    ns3::NetDeviceContainer devices;
    devices = csma.Install(nodes);

    // Install Internet stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from 10.1.1.0/24 range
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Echo Server on the last node (node 3)
    uint16_t port = 9; // Default echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Setup UDP Echo Clients on the other nodes (nodes 0, 1, 2)
    // The target address for clients is the IP address of the server node (node 3)
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(3);

    ns3::UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10)); // 10 packets (1s to 10s, at 1s intervals)
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(512));

    ns3::ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 3; ++i) // Install clients on nodes 0, 1, 2
    {
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(ns3::Seconds(1.0));
    clientApps.Stop(ns3::Seconds(10.0));

    // Populate routing tables (important for multi-segment networks, harmless for single segment)
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set overall simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}