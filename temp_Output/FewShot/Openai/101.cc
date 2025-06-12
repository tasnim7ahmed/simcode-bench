#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for debugging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: n0, n1, n2, n3
    NodeContainer nodes;
    nodes.Create(4);

    // Node containers for each p2p link
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));

    // Configure the p2p links and set DropTail
    PointToPointHelper p2p0n2;
    p2p0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p0n2.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p0n2.SetQueue("ns3::DropTailQueue");

    PointToPointHelper p2p1n2;
    p2p1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1n2.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p1n2.SetQueue("ns3::DropTailQueue");

    PointToPointHelper p2p2n3;
    p2p2n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p2n3.SetChannelAttribute("Delay", StringValue("10ms"));
    p2p2n3.SetQueue("ns3::DropTailQueue");

    // Install devices
    NetDeviceContainer dev0n2 = p2p0n2.Install(n0n2);
    NetDeviceContainer dev1n2 = p2p1n2.Install(n1n2);
    NetDeviceContainer dev2n3 = p2p2n3.Install(n2n3);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if0n2 = address.Assign(dev0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if1n2 = address.Assign(dev1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if2n3 = address.Assign(dev2n3);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Trace file
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("simple-global-routing.tr");
    p2p0n2.EnableAsciiAll(stream);
    p2p1n2.EnableAsciiAll(stream);
    p2p2n3.EnableAsciiAll(stream);

    // --- CBR UDP traffic n0 -> n3 ---

    uint16_t cbrPort1 = 5000;
    OnOffHelper onoff1("ns3::UdpSocketFactory",
        InetSocketAddress(if2n3.GetAddress(1), cbrPort1));
    onoff1.SetAttribute("PacketSize", UintegerValue(210));
    onoff1.SetAttribute("DataRate", StringValue("448kbps"));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(2.0)));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer cbrApp1 = onoff1.Install(nodes.Get(0));

    PacketSinkHelper sink1("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), cbrPort1));
    ApplicationContainer sinkApp1 = sink1.Install(nodes.Get(3));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(3.0));

    // --- CBR UDP traffic n3 -> n1 ---

    uint16_t cbrPort2 = 5001;
    OnOffHelper onoff2("ns3::UdpSocketFactory",
        InetSocketAddress(if1n2.GetAddress(0), cbrPort2));
    onoff2.SetAttribute("PacketSize", UintegerValue(210));
    onoff2.SetAttribute("DataRate", StringValue("448kbps"));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(2.0)));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer cbrApp2 = onoff2.Install(nodes.Get(3));

    PacketSinkHelper sink2("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), cbrPort2));
    ApplicationContainer sinkApp2 = sink2.Install(nodes.Get(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(3.0));

    // --- FTP/TCP flow n0 -> n3 ---
    uint16_t ftpPort = 21;

    Address ftpAddress(InetSocketAddress(if2n3.GetAddress(1), ftpPort));
    PacketSinkHelper ftpSinkHelper("ns3::TcpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), ftpPort));
    ApplicationContainer ftpSinkApp = ftpSinkHelper.Install(nodes.Get(3));
    ftpSinkApp.Start(Seconds(0.0));
    ftpSinkApp.Stop(Seconds(3.0));

    OnOffHelper ftpClientHelper("ns3::TcpSocketFactory", ftpAddress);
    ftpClientHelper.SetAttribute("PacketSize", UintegerValue(210));
    ftpClientHelper.SetAttribute("DataRate", StringValue("1Mbps"));
    ftpClientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    ftpClientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer ftpClientApp = ftpClientHelper.Install(nodes.Get(0));
    ftpClientApp.Start(Seconds(1.2));
    ftpClientApp.Stop(Seconds(1.35));

    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}