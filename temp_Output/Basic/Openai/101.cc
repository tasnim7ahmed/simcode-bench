#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4); // n0, n1, n2, n3

    // Point-to-Point links
    PointToPointHelper p2pA, p2pB, p2pC;
    p2pA.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pA.SetChannelAttribute("Delay", StringValue("2ms"));
    p2pA.SetQueue("ns3::DropTailQueue");

    p2pB.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pB.SetChannelAttribute("Delay", StringValue("2ms"));
    p2pB.SetQueue("ns3::DropTailQueue");

    p2pC.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2pC.SetChannelAttribute("Delay", StringValue("10ms"));
    p2pC.SetQueue("ns3::DropTailQueue");

    // Set up point-to-point connections
    NetDeviceContainer d0d2 = p2pA.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer d1d2 = p2pB.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2d3 = p2pC.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- CBR UDP Traffic: n0 -> n3 ---
    uint16_t portCBR1 = 8000;
    UdpServerHelper udpServer1(portCBR1);
    ApplicationContainer serverApp1 = udpServer1.Install(nodes.Get(3));
    serverApp1.Start(Seconds(0.0));
    serverApp1.Stop(Seconds(10.0));

    UdpClientHelper udpClient1(i2i3.GetAddress(1), portCBR1);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient1.SetAttribute("Interval", TimeValue(Seconds(double(210.0 * 8)/448000.0))); // 448 kbps, 210 bytes
    udpClient1.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApp1 = udpClient1.Install(nodes.Get(0));
    clientApp1.Start(Seconds(0.0));
    clientApp1.Stop(Seconds(10.0));

    // --- CBR UDP Traffic: n3 -> n1 ---
    uint16_t portCBR2 = 8001;
    UdpServerHelper udpServer2(portCBR2);
    ApplicationContainer serverApp2 = udpServer2.Install(nodes.Get(1));
    serverApp2.Start(Seconds(0.0));
    serverApp2.Stop(Seconds(10.0));

    UdpClientHelper udpClient2(i1i2.GetAddress(0), portCBR2);
    udpClient2.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient2.SetAttribute("Interval", TimeValue(Seconds(double(210.0 * 8)/448000.0)));
    udpClient2.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApp2 = udpClient2.Install(nodes.Get(3));
    clientApp2.Start(Seconds(0.0));
    clientApp2.Stop(Seconds(10.0));

    // --- FTP/TCP Traffic: n0 -> n3 ---
    uint16_t portFTP = 9000;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), portFTP));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portFTP));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer bulkApp = bulkSendHelper.Install(nodes.Get(0));
    bulkApp.Start(Seconds(1.2));
    bulkApp.Stop(Seconds(1.35));

    // Enable tracing
    AsciiTraceHelper ascii;
    p2pA.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2pB.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2pC.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}