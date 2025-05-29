#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install packet sink on Node 1 (receiver)
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.));
    sinkApp.Stop(Seconds(10.));

    // Install BulkSendApplication on Node 0 (sender)
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer senderApp = bulkSender.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.));
    senderApp.Stop(Seconds(10.));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp-simple");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}