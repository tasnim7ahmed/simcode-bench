#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    // (n0-n2)
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    // (n1-n2)
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    // (n2-n3)
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    // Point-to-point Helper configs
    PointToPointHelper p2p_5m_2ms;
    p2p_5m_2ms.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_5m_2ms.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p_5m_2ms.SetQueue("ns3::DropTailQueue");

    PointToPointHelper p2p_1_5m_10ms;
    p2p_1_5m_10ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_1_5m_10ms.SetChannelAttribute("Delay", StringValue("10ms"));
    p2p_1_5m_10ms.SetQueue("ns3::DropTailQueue");

    // Install devices
    NetDeviceContainer d0d2 = p2p_5m_2ms.Install(n0n2);
    NetDeviceContainer d1d2 = p2p_5m_2ms.Install(n1n2);
    NetDeviceContainer d2d3 = p2p_1_5m_10ms.Install(n2n3);

    // Install the Internet stack
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

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // CBR/UDP: n0 -> n3, n3 -> n1
    // n0 -> n3
    uint16_t udpPort1 = 4000;
    UdpServerHelper udpServer1(udpPort1);
    ApplicationContainer serverApp1 = udpServer1.Install(nodes.Get(3));
    serverApp1.Start(Seconds(0.0));
    serverApp1.Stop(Seconds(5.0));

    UdpClientHelper udpClient1(i2i3.GetAddress(1), udpPort1);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(3200));
    udpClient1.SetAttribute("Interval", TimeValue(Seconds(210.0 * 8.0 / 448000.0)));
    udpClient1.SetAttribute("PacketSize", UintegerValue(210));
    udpClient1.SetAttribute("StartTime", TimeValue(Seconds(0.0)));
    udpClient1.SetAttribute("StopTime", TimeValue(Seconds(5.0)));

    ApplicationContainer clientApp1 = udpClient1.Install(nodes.Get(0));
    clientApp1.Start(Seconds(0.0));
    clientApp1.Stop(Seconds(5.0));

    // n3 -> n1
    uint16_t udpPort2 = 5000;
    UdpServerHelper udpServer2(udpPort2);
    ApplicationContainer serverApp2 = udpServer2.Install(nodes.Get(1));
    serverApp2.Start(Seconds(0.0));
    serverApp2.Stop(Seconds(5.0));

    UdpClientHelper udpClient2(i1i2.GetAddress(0), udpPort2);
    udpClient2.SetAttribute("MaxPackets", UintegerValue(3200));
    udpClient2.SetAttribute("Interval", TimeValue(Seconds(210.0 * 8.0 / 448000.0)));
    udpClient2.SetAttribute("PacketSize", UintegerValue(210));
    udpClient2.SetAttribute("StartTime", TimeValue(Seconds(0.0)));
    udpClient2.SetAttribute("StopTime", TimeValue(Seconds(5.0)));

    ApplicationContainer clientApp2 = udpClient2.Install(nodes.Get(3));
    clientApp2.Start(Seconds(0.0));
    clientApp2.Stop(Seconds(5.0));

    // FTP/TCP: n0 -> n3
    uint16_t tcpPort = 21;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), tcpPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(5.0));

    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
    bulkSender.SetAttribute("SendSize", UintegerValue(210));

    ApplicationContainer ftpApp = bulkSender.Install(nodes.Get(0));
    ftpApp.Start(Seconds(1.2));
    ftpApp.Stop(Seconds(1.35));

    // Enable tracing
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("simple-global-routing.tr");
    p2p_5m_2ms.EnableAsciiAll(stream);
    p2p_1_5m_10ms.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}