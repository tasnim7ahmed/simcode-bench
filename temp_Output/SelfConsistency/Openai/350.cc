#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up the point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the TCP server (PacketSink) on Node 0
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the TCP client (BulkSendApplication is suitable for a fixed number of bytes)
    uint32_t numPackets = 10;
    uint32_t packetSize = 1024; // bytes
    uint32_t maxBytes = numPackets * packetSize;

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory",
                                  InetSocketAddress(interfaces.GetAddress(0), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Schedule app traffic at specified intervals (10 packets, 1 per second)
    // BulkSendApplication will try to send as fast as possible.
    // To strictly send one packet per second, use a custom application or OnOffApplication.

    // Alternative: Use OnOffApplication to have more control over interval.
    /*
    OnOffHelper onoff("ns3::TcpSocketFactory",
                      InetSocketAddress(interfaces.GetAddress(0), port));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", StringValue("8192bps")); // Will be overridden by send interval
    onoff.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(12.0));
    */

    // Enable pcap tracing (optional)
    // pointToPoint.EnablePcapAll("tcp-point-to-point");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}