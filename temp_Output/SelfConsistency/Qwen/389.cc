#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSimpleExample");

int main(int argc, char *argv[])
{
    // Enable logging for UdpEchoClient and UdpEchoServer
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Server (Node 1) listens on port 9
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

    // Set up packet sink on server node
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(11.0));

    // Set up TCP client application on node 0
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // send forever until stopped

    ApplicationContainer clientApps = bulkSend.Install(nodes.Get(0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(10.0));

    // Set constant packet size of 1024 bytes every millisecond
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
    Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(0)));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4 * 1024 * 1024));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4 * 1024 * 1024));

    // Enable ASCII tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tcp-simple-example.tr");
    pointToPoint.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}