#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PTcpExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("P2PTcpExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Point-to-Point (P2P) link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install P2P devices
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup TCP server on Node 1
    uint16_t port = 5000;
    PacketSinkHelper tcpServer("ns3::TcpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup TCP client on Node 0
    OnOffHelper tcpClient("ns3::TcpSocketFactory",
                          InetSocketAddress(interfaces.GetAddress(1), port));
    tcpClient.SetAttribute("DataRate", StringValue("5Mbps"));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    pointToPoint.EnablePcapAll("p2p-tcp");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
