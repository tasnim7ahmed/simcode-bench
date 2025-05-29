#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP sink on node 1 (receiver)
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP bulk sender on node 0 (sender)
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    pointToPoint.EnablePcapAll("tcp-p2p");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}