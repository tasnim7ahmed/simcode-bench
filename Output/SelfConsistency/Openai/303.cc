#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for debugging (optional)
    // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

    // Step 1: Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Step 2: Configure point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Step 3: Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Step 4: Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Step 5: Install and configure TCP server (PacketSink) on Node 1
    uint16_t port = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);

    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Step 6: Install and configure TCP client (BulkSendApplication) on Node 0
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(500000)); // 500,000 bytes
    ApplicationContainer sourceApp = bulkSender.Install(nodes.Get(0));
    sourceApp.Start(Seconds(2.0));
    sourceApp.Stop(Seconds(10.0));

    // Optional: Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp-point-to-point");

    // Step 7: Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}