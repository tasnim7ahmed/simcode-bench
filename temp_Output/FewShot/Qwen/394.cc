#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes (Client and Server)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link with data rate, delay, and packet loss
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    // Introduce 0.1% packet loss using DropTailQueue
    QueueSizeValue queueSize(QueueSize("100p"));
    pointToPoint.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", queueSize);

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server (use ns-3's BulkSendApplication as a TCP server)
    uint16_t port = 50000;
    BulkSendHelper tcpServer("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client to send packets
    OnOffHelper tcpClient("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    tcpClient.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    tcpClient.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    tcpClient.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    tcpClient.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite sending

    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}