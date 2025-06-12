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

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint02;
    pointToPoint02.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint02.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper pointToPoint12;
    pointToPoint12.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint12.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper pointToPoint23;
    pointToPoint23.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPoint23.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices02 = pointToPoint02.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices12 = pointToPoint12.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = pointToPoint23.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // CBR from n0 to n3
    uint16_t portCbr03 = 12345;
    UdpClientHelper clientHelper03(interfaces23.GetAddress(1), portCbr03);
    clientHelper03.SetAttribute("PacketSize", UintegerValue(210));
    clientHelper03.SetAttribute("Interval", TimeValue(DataRate("448kbps") / (210 * 8)));
    ApplicationContainer clientApps03 = clientHelper03.Install(nodes.Get(0));
    clientApps03.Start(Seconds(1.0));
    clientApps03.Stop(Seconds(1.5));

    UdpServerHelper serverHelper03(portCbr03);
    ApplicationContainer serverApps03 = serverHelper03.Install(nodes.Get(3));
    serverApps03.Start(Seconds(0.5));
    serverApps03.Stop(Seconds(2.0));

    // CBR from n3 to n1
    uint16_t portCbr31 = 12346;
    UdpClientHelper clientHelper31(interfaces12.GetAddress(0), portCbr31);
    clientHelper31.SetAttribute("PacketSize", UintegerValue(210));
    clientHelper31.SetAttribute("Interval", TimeValue(DataRate("448kbps") / (210 * 8)));
    ApplicationContainer clientApps31 = clientHelper31.Install(nodes.Get(3));
    clientApps31.Start(Seconds(1.1));
    clientApps31.Stop(Seconds(1.6));

    UdpServerHelper serverHelper31(portCbr31);
    ApplicationContainer serverApps31 = serverHelper31.Install(nodes.Get(1));
    serverApps31.Start(Seconds(0.6));
    serverApps31.Stop(Seconds(2.1));


    // FTP from n0 to n3
    uint16_t portFtp03 = 2000;
    BulkSendHelper ftpClientHelper(interfaces23.GetAddress(1), portFtp03);
    ftpClientHelper.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer ftpClientApps = ftpClientHelper.Install(nodes.Get(0));
    ftpClientApps.Start(Seconds(1.2));
    ftpClientApps.Stop(Seconds(1.35));

    PacketSinkHelper ftpServerHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), portFtp03));
    ApplicationContainer ftpServerApps = ftpServerHelper.Install(nodes.Get(3));
    ftpServerApps.Start(Seconds(1.1));
    ftpServerApps.Stop(Seconds(1.4));

    Simulator::Stop(Seconds(2.5));

    AsciiTraceHelper ascii;
    pointToPoint02.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    pointToPoint12.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    pointToPoint23.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}