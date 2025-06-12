#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleGlobalRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p02;
    p2p02.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p02.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p12.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p23.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d0d2 = p2p02.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer d1d2 = p2p12.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2d3 = p2p23.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t packetSize = 210;
    Time interPacketInterval = Seconds(0.03); // 210 bytes at 448kbps = 0.03 seconds per packet

    UdpClientHelper udpClient03(i2i3.GetAddress(1), 9);
    udpClient03.SetAttribute("PacketSize", UintegerValue(packetSize));
    udpClient03.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    udpClient03.SetAttribute("Interval", TimeValue(interPacketInterval));

    ApplicationContainer clientApps03 = udpClient03.Install(nodes.Get(0));
    clientApps03.Start(Seconds(0.0));
    clientApps03.Stop(Seconds(2.0));

    PacketSinkHelper sink0 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
    ApplicationContainer sinkApps03 = sink0.Install(nodes.Get(3));
    sinkApps03.Start(Seconds(0.0));
    sinkApps03.Stop(Seconds(2.0));

    UdpClientHelper udpClient31(i1i2.GetAddress(0), 9);
    udpClient31.SetAttribute("PacketSize", UintegerValue(packetSize));
    udpClient31.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    udpClient31.SetAttribute("Interval", TimeValue(interPacketInterval));

    ApplicationContainer clientApps31 = udpClient31.Install(nodes.Get(3));
    clientApps31.Start(Seconds(0.0));
    clientApps31.Stop(Seconds(2.0));

    PacketSinkHelper sink1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
    ApplicationContainer sinkApps31 = sink1.Install(nodes.Get(1));
    sinkApps31.Start(Seconds(0.0));
    sinkApps31.Stop(Seconds(2.0));

    uint16_t sinkPort = 20;
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(2.0));

    BulkSendHelper ftpHelper("ns3::TcpSocketFactory", sinkAddress);
    ftpHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer ftpApp = ftpHelper.Install(nodes.Get(0));
    ftpApp.Start(Seconds(1.2));
    ftpApp.Stop(Seconds(1.35));

    AsciiTraceHelper ascii;
    p2p02.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p12.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p23.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}