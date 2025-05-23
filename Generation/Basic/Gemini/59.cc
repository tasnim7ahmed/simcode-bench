#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("TcpBulkSendSimulation"); // Not allowed by the prompt, only code

int
main(int argc, char* argv[])
{
    uint64_t maxBytes = 0; // Default to 0, which means unlimited
    bool tracing = false;

    // Command line argument parsing
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("maxBytes", "Maximum bytes for BulkSendApplication to send (0 for unlimited)", maxBytes);
    cmd.AddValue("tracing", "Enable tracing (ASCII and PCAP) for the simulation", tracing);
    cmd.Parse(argc, argv);

    // Set default TCP type to NewReno
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpNewReno"));

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Create a PointToPoint helper and set attributes
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("500Kbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("5ms"));

    // Install devices on nodes
    ns3::NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // Install internet stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // BulkSendApplication (sender)
    uint16_t port = 9; // Echo port
    ns3::BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                                 ns3::Address(ns3::InetSocketAddress(interfaces.GetAddress(1), port)));
    // Set the MaxBytes attribute from the command line
    bulkSend.SetAttribute("MaxBytes", ns3::UintegerValue(maxBytes));
    bulkSend.SetAttribute("SendSize", ns3::UintegerValue(1000)); // Default send size of 1000 bytes

    ns3::ApplicationContainer senderApps = bulkSend.Install(nodes.Get(0));
    senderApps.Start(ns3::Seconds(0.0));
    senderApps.Stop(ns3::Seconds(10.0));

    // PacketSinkApplication (receiver)
    ns3::PacketSinkHelper packetSink("ns3::TcpSocketFactory",
                                     ns3::Address(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port)));
    ns3::ApplicationContainer receiverApps = packetSink.Install(nodes.Get(1));
    receiverApps.Start(ns3::Seconds(0.0));
    receiverApps.Stop(ns3::Seconds(10.0));

    // Populate routing tables
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable tracing if requested
    if (tracing)
    {
        p2p.EnableAsciiAll("tcp-bulk-send.tr");
        p2p.EnablePcapAll("tcp-bulk-send", true); // true for promiscuous mode
    }

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    // Get total bytes received by the sink
    ns3::Ptr<ns3::PacketSink> sink = ns3::DynamicCast<ns3::PacketSink>(receiverApps.Get(0));
    uint64_t totalBytesReceived = sink->GetTotalRx();
    std::cout << "Total bytes received by sink: " << totalBytesReceived << std::endl;

    return 0;
}