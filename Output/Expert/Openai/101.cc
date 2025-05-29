#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Point-to-Point helpers
    PointToPointHelper p2p05;
    p2p05.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p05.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p05.SetQueue("ns3::DropTailQueue");

    PointToPointHelper p2p15;
    p2p15.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p15.SetChannelAttribute("Delay", StringValue("10ms"));
    p2p15.SetQueue("ns3::DropTailQueue");

    // Link n0 - n2
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev0_2 = p2p05.Install(n0n2);

    // Link n1 - n2
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev1_2 = p2p05.Install(n1n2);

    // Link n2 - n3
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer dev2_3 = p2p15.Install(n2n3);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(dev0_2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(dev1_2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(dev2_3);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // CBR/UDP: n0 -> n3
    uint16_t udpPort1 = 4000;
    UdpServerHelper udpServer1(udpPort1);
    ApplicationContainer serverApp1 = udpServer1.Install(nodes.Get(3));
    serverApp1.Start(Seconds(0.0));
    serverApp1.Stop(Seconds(2.0));

    UdpClientHelper udpClient1(i2i3.GetAddress(1), udpPort1);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient1.SetAttribute("Interval", TimeValue(Seconds(210.0 * 8 / 448000.0)));
    udpClient1.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApp1 = udpClient1.Install(nodes.Get(0));
    clientApp1.Start(Seconds(0.0));
    clientApp1.Stop(Seconds(2.0));

    // CBR/UDP: n3 -> n1
    uint16_t udpPort2 = 4001;
    UdpServerHelper udpServer2(udpPort2);
    ApplicationContainer serverApp2 = udpServer2.Install(nodes.Get(1));
    serverApp2.Start(Seconds(0.0));
    serverApp2.Stop(Seconds(2.0));

    UdpClientHelper udpClient2(i1i2.GetAddress(0), udpPort2);
    udpClient2.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient2.SetAttribute("Interval", TimeValue(Seconds(210.0 * 8 / 448000.0)));
    udpClient2.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApp2 = udpClient2.Install(nodes.Get(3));
    clientApp2.Start(Seconds(0.0));
    clientApp2.Stop(Seconds(2.0));

    // FTP/TCP: n0 -> n3
    uint16_t tcpPort = 5000;
    Address tcpSinkAddress(InetSocketAddress(i2i3.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpSinkAddress);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(nodes.Get(3));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(2.0));

    OnOffHelper ftpApp("ns3::TcpSocketFactory", tcpSinkAddress);
    ftpApp.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    ftpApp.SetAttribute("PacketSize", UintegerValue(210));
    ftpApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    ftpApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer ftpSourceApp = ftpApp.Install(nodes.Get(0));
    ftpSourceApp.Start(Seconds(1.2));
    ftpSourceApp.Stop(Seconds(1.35));

    // ASCII Tracing
    AsciiTraceHelper ascii;
    p2p05.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p15.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}