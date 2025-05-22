#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointTcpExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("PointToPointTcpExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link with specific data rate and delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the internet stack on both nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up a BulkSendApplication on the sender (node 0)
    uint16_t port = 9; // TCP well-known port number
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
    ApplicationContainer senderApp = bulkSend.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Set up a PacketSinkApplication on the receiver (node 1)
    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer receiverApp = packetSink.Install(nodes.Get(1));
    receiverApp.Start(Seconds(1.0));
    receiverApp.Stop(Seconds(10.0));

    // Enable packet tracing (pcap file generation)
    pointToPoint.EnablePcapAll("tcp-p2p-example");

    // Run the simulation
    Simulator::Run();

    // Log the total bytes received by the receiver node
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(receiverApp.Get(0));
    NS_LOG_INFO("Total Bytes Received: " << sink->GetTotalRx());

    // Destroy the simulation
    Simulator::Destroy();

    return 0;
}

