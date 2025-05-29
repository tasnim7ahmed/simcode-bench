#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkSendExample");

int
main(int argc, char *argv[])
{
    uint32_t maxBytes = 0; // 0 means unlimited
    bool enableTracing = false;

    CommandLine cmd;
    cmd.AddValue("maxBytes", "Total number of bytes to send (0 = unlimited)", maxBytes);
    cmd.AddValue("tracing", "Enable ASCII and PCAP tracing", enableTracing);
    cmd.Parse(argc, argv);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure the point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install PacketSinkApplication on node 1
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install BulkSendApplication on node 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer senderApp = bulkSender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Enable tracing if requested
    if (enableTracing) {
        AsciiTraceHelper ascii;
        pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-bulk-send.tr"));
        pointToPoint.EnablePcapAll("tcp-bulk-send", true);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print the total bytes received by the sink
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    std::cout << "Total Bytes Received: " << sink->GetTotalRx() << std::endl;

    Simulator::Destroy();
    return 0;
}