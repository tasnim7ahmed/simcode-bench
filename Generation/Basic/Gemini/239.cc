#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // Configure default TCP parameters
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpNewReno"));
    ns3::Config::SetDefault("ns3::TcpSocket::SegmentSize", ns3::UintegerValue(1460));

    // Create three nodes: client, router, server
    ns3::NodeContainer nodes;
    nodes.Create(3);
    ns3::Ptr<ns3::Node> clientNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> routerNode = nodes.Get(1);
    ns3::Ptr<ns3::Node> serverNode = nodes.Get(2);

    // Create PointToPointHelper for links
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install NetDevices on client-router link
    ns3::NetDeviceContainer clientRouterDevices;
    clientRouterDevices = p2pHelper.Install(clientNode, routerNode);

    // Install NetDevices on router-server link
    ns3::NetDeviceContainer routerServerDevices;
    routerServerDevices = p2pHelper.Install(routerNode, serverNode);

    // Install Internet Stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP Addresses
    ns3::Ipv4AddressHelper ipv4;

    // Assign IP addresses to client-router link
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer clientRouterInterfaces = ipv4.Assign(clientRouterDevices);

    // Assign IP addresses to router-server link
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer routerServerInterfaces = ipv4.Assign(routerServerDevices);

    // Enable Global Routing to automatically populate routing tables
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure and install Server Application (PacketSink)
    uint16_t port = 9; // Port for the server to listen on
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(serverNode);
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Configure and install Client Application (OnOffApplication)
    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory", ns3::Address()); // Remote address set later
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always On
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never Off
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Size of data packets
    onoffHelper.SetAttribute("DataRate", ns3::StringValue("1Mbps")); // Data rate of the client

    // Get the server's IP address from the router-server interface container
    ns3::Ipv4Address serverIpAddress = routerServerInterfaces.GetAddress(1);
    ns3::Address serverAddress(ns3::InetSocketAddress(serverIpAddress, port));
    onoffHelper.SetAttribute("Remote", ns3::AddressValue(serverAddress));

    ns3::ApplicationContainer clientApps = onoffHelper.Install(clientNode);
    clientApps.Start(ns3::Seconds(1.0)); // Start client after server
    clientApps.Stop(ns3::Seconds(9.0));

    // Enable PCAP tracing for all Point-to-Point devices
    p2pHelper.EnablePcapAll("p2p-tcp-router");

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}