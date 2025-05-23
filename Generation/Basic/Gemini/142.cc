#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE("StarTopologySimulation"); // This is a comment, remove it.

int main(int argc, char *argv[]) {
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    // Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(3);
    ns3::Ptr<ns3::Node> centralNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> peripheral1Node = nodes.Get(1);
    ns3::Ptr<ns3::Node> peripheral2Node = nodes.Get(2);

    // Configure Point-to-Point links
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("1ms"));

    // Install devices and connect nodes
    ns3::NetDeviceContainer devices1 = p2p.Install(centralNode, peripheral1Node);
    ns3::NetDeviceContainer devices2 = p2p.Install(centralNode, peripheral2Node);

    // Install Internet stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    // Populate routing tables (GlobalRouting is usually enabled with InternetStackHelper)
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP Echo Server on Peripheral Node 2
    ns3::UdpEchoServerHelper echoServer(9); // Port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(peripheral2Node);
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(12.0));

    // Setup UDP Echo Client on Peripheral Node 1
    // Target IP is Peripheral Node 2's IP address on its link to the central node
    ns3::UdpEchoClientHelper echoClient(interfaces2.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(peripheral1Node);
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(12.0));

    // Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}