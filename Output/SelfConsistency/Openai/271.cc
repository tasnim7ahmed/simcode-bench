#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable command-line arguments parsing (optional)
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point devices on nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the PacketSink on node 1 (receiver)
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Set up a BulkSendApplication on node 0 (sender)
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

    ApplicationContainer sourceApps = bulkSendHelper.Install(nodes.Get(0));
    sourceApps.Start(Seconds(0.0));
    sourceApps.Stop(Seconds(10.0));

    // Enable pcap tracing if needed
    // pointToPoint.EnablePcapAll("point-to-point-tcp");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}