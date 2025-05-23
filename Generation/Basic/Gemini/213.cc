#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <iostream>

using namespace ns3;

int
main()
{
    // Set default TCP congestion control algorithm to NewReno
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    // Enable logging for applications to see their behavior
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO); // Useful for TCP state transitions

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create a point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the TCP server (PacketSink) on Node 1
    uint16_t port = 9; // Port for the server
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(1)); // Node 1 is the server
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0)); // Server runs for the entire simulation

    // Set up the TCP client (OnOffApplication) on Node 0
    // The client will send data to the server's IP address and port
    OnOffHelper onoffHelper("ns3::TcpSocketFactory",
                            InetSocketAddress(interfaces.GetAddress(1), port)); // Connect to Node 1's IP
    onoffHelper.SetAttribute("OnTime",
                             StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always On
    onoffHelper.SetAttribute("OffTime",
                             StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never Off
    onoffHelper.SetAttribute("DataRate", StringValue("1Mbps"));                      // Client sends at 1 Mbps
    onoffHelper.SetAttribute("PacketSize", UintegerValue(1024));                    // Packet size of 1 KB

    ApplicationContainer clientApps = onoffHelper.Install(nodes.Get(0)); // Node 0 is the client
    clientApps.Start(Seconds(1.0));                                      // Client starts sending at 1 second
    clientApps.Stop(Seconds(9.0));                                       // Client stops sending at 9 seconds

    // Enable PCAP tracing on all devices
    pointToPoint.EnablePcapAll("tcp-simulation");

    // Set the simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Retrieve and print statistics
    Ptr<PacketSink> serverSink = DynamicCast<PacketSink>(serverApps.Get(0));
    uint64_t totalRxBytes = serverSink->GetTotalRx();

    Ptr<OnOffApplication> clientApp = DynamicCast<OnOffApplication>(clientApps.Get(0));
    uint64_t totalTxBytes = clientApp->GetTotalTxBytes();

    std::cout << "\n--- Simulation Results ---" << std::endl;
    std::cout << "Total bytes transmitted by client (Node 0): " << totalTxBytes << " bytes" << std::endl;
    std::cout << "Total bytes received by server (Node 1): " << totalRxBytes << " bytes" << std::endl;
    std::cout << "--------------------------" << std::endl;

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}