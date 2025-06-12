#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create two nodes (Client and Server)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses in 192.168.1.0/24 subnet
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Ensure server is at 192.168.1.2
    NS_ASSERT(interfaces.GetAddress(1) == Ipv4Address("192.168.1.2"));

    // Set up TCP Server (PacketSink)
    uint16_t port = 9;  // Well-known port
    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSink.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP Client (OnOffApplication)
    OnOffHelper onOff("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onOff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    onOff.SetAttribute("PacketSize", UintegerValue(1024));
    onOff.SetAttribute("MaxBytes", UintegerValue(0));  // Infinite packets

    ApplicationContainer clientApp = onOff.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}