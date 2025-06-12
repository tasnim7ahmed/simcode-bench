#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    NetDeviceContainer devices02 = p2p.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p23.SetChannelAttribute("Delay", StringValue("10ms"));
    p2p23.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    NetDeviceContainer devices23 = p2p23.Install(nodes.Get(2), nodes.Get(3));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP/CBR traffic from n0 to n3
    uint16_t port03 = 9;
    UdpServerHelper server03(port03);
    server03.SetAttribute("ReceiveBufferSize", UintegerValue(65535));
    ApplicationContainer serverApps03 = server03.Install(nodes.Get(3));
    serverApps03.Start(Seconds(1.0));
    serverApps03.Stop(Seconds(5.0));

    UdpClientHelper client03(interfaces23.GetAddress(1), port03);
    client03.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client03.SetAttribute("Interval", TimeValue(Time("210us")));
    client03.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApps03 = client03.Install(nodes.Get(0));
    clientApps03.Start(Seconds(1.0));
    clientApps03.Stop(Seconds(5.0));

    // UDP/CBR traffic from n3 to n1
    uint16_t port31 = 10;
    UdpServerHelper server31(port31);
    server31.SetAttribute("ReceiveBufferSize", UintegerValue(65535));
    ApplicationContainer serverApps31 = server31.Install(nodes.Get(1));
    serverApps31.Start(Seconds(1.0));
    serverApps31.Stop(Seconds(5.0));

    UdpClientHelper client31(interfaces12.GetAddress(0), port31);
    client31.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client31.SetAttribute("Interval", TimeValue(Time("210us")));
    client31.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApps31 = client31.Install(nodes.Get(3));
    clientApps31.Start(Seconds(1.0));
    clientApps31.Stop(Seconds(5.0));

    // FTP/TCP traffic from n0 to n3
    uint16_t port = 50000;
    BulkSendHelper ftpClient("ns3::TcpSocketFactory", InetSocketAddress(interfaces23.GetAddress(1), port));
    ftpClient.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer ftpClientApps = ftpClient.Install(nodes.Get(0));
    ftpClientApps.Start(Seconds(1.2));
    ftpClientApps.Stop(Seconds(1.35));

    PacketSinkHelper ftpServer("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer ftpServerApps = ftpServer.Install(nodes.Get(3));
    ftpServerApps.Start(Seconds(1.0));
    ftpServerApps.Stop(Seconds(5.0));

    Simulator::Stop(Seconds(5.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}