#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP server on node 1 (second node)
    uint16_t tcpPort = 50000;
    Address tcpLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpLocalAddress);
    ApplicationContainer tcpServerApp = tcpSinkHelper.Install(nodes.Get(1));
    tcpServerApp.Start(Seconds(0.));
    tcpServerApp.Stop(Seconds(10.));

    // Install TCP client on node 0 (first node)
    BulkSendHelper tcpClientHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    tcpClientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited (let it send until simulation ends)
    ApplicationContainer tcpClientApp = tcpClientHelper.Install(nodes.Get(0));
    tcpClientApp.Start(Seconds(1.));
    tcpClientApp.Stop(Seconds(10.));

    // Install UDP server on node 1
    uint16_t udpPort = 40000;
    UdpServerHelper udpServerHelper(udpPort);
    ApplicationContainer udpServerApp = udpServerHelper.Install(nodes.Get(1));
    udpServerApp.Start(Seconds(0.));
    udpServerApp.Stop(Seconds(10.));

    // Install UDP client on node 0
    UdpClientHelper udpClientHelper(interfaces.GetAddress(1), udpPort);
    udpClientHelper.SetAttribute("MaxPackets", UintegerValue(100));
    udpClientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    udpClientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer udpClientApp = udpClientHelper.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(2.));
    udpClientApp.Stop(Seconds(10.));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("tcp-udp-two-nodes");

    Simulator::Stop(Seconds(10.));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}