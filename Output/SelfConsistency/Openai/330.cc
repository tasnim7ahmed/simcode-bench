#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure PointToPoint link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install server application (PacketSink acts as HTTP server placeholder on port 80)
    uint16_t serverPort = 80;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);

    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install client application (BulkSend emulates HTTP GET/POST traffic)
    BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(1), serverPort));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited, as a simple example

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing (optional)
    pointToPoint.EnablePcapAll("http-simple-wired");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}