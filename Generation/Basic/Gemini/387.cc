#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

int main(int argc, char* argv[])
{
    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Create a Point-to-Point link between the nodes
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack on the nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up mobility: nodes positioned at 0,0 and 5,5
    // ConstantPositionMobilityModel is used to place nodes at fixed coordinates.
    // While the description mentions "grid mobility model", it explicitly states fixed positions.
    // ConstantPositionMobilityModel directly achieves this static placement.
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    
    // Install mobility model on Node 0 and set its position
    mobility.Install(nodes.Get(0));
    nodes.Get(0)->GetObject<ns3::MobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));

    // Install mobility model on Node 1 and set its position
    mobility.Install(nodes.Get(1));
    nodes.Get(1)->GetObject<ns3::MobilityModel>()->SetPosition(ns3::Vector(5.0, 5.0, 0.0));

    // Server setup (Node 1)
    uint16_t port = 8080;
    ns3::Address sinkAddress(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(ns3::Seconds(1.0)); // Server starts listening at 1s
    sinkApps.Stop(ns3::Seconds(10.0)); // Server stops at 10s

    // Client setup (Node 0)
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(1); // IP address of Node 1
    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory", ns3::Address()); // Placeholder address
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1 KB packets
    onoffHelper.SetAttribute("DataRate", ns3::StringValue("1Mbps")); // Example data rate

    // Set the remote address (server IP and port) for the OnOff application
    ns3::AddressValue remoteAddress(ns3::InetSocketAddress(serverAddress, port));
    onoffHelper.SetAttribute("Remote", remoteAddress);

    ns3::ApplicationContainer clientApps = onoffHelper.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0)); // Client starts sending at 1s
    clientApps.Stop(ns3::Seconds(9.0)); // Client stops sending at 9s to ensure data reaches server by 10s

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}