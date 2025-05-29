#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPointToPointExample");

// Throughput calculation variables
uint64_t totalBytesReceived = 0;
Time throughputStartTime = Seconds(1.0);
Time throughputEndTime = Seconds(10.0);

// Callback to count bytes received
void RxCallback(Ptr<const Packet> packet, const Address &)
{
    totalBytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    // Logging (uncomment to enable debugging)
    //LogComponentEnable("TcpPointToPointExample", LOG_LEVEL_INFO);
    //LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    //LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Create point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // 3. Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 5. Configure TCP server (PacketSink) on Node A
    uint16_t port = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);

    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(11.0));

    // 6. Configure TCP client (BulkSendApplication) on Node B
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited send

    ApplicationContainer clientApp = bulkSender.Install(nodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // 7. Enable tracing
    pointToPoint.EnablePcapAll("tcp-p2p");
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-p2p.tr"));

    // 8. Throughput calculation: connect to sink
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    double throughput = (totalBytesReceived * 8.0) / (throughputEndTime - throughputStartTime).GetSeconds(); // bits per second

    std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;
    std::cout << "Measured Throughput: " << throughput / 1e6 << " Mbps" << std::endl;

    Simulator::Destroy();

    return 0;
}