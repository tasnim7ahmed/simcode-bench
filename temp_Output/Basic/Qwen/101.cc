#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourNodeSimulation");

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

    PointToPointHelper p12;
    p12.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p12.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p23;
    p23.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p23.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer dev02 = p2p02.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev12 = p12.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev23 = p23.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if02 = address.Assign(dev02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if12 = address.Assign(dev12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = address.Assign(dev23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    // CBR over UDP from n0 to n3
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(if23.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("448kbps"), 210);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(0.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer app0n0 = onoff.Install(nodes.Get(0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer app0sink = sink.Install(nodes.Get(3));
    app0sink.Start(Seconds(0.0));
    app0sink.Stop(Seconds(10.0));

    // CBR over UDP from n3 to n1
    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(if12.GetAddress(1), port)));
    onoff1.SetConstantRate(DataRate("448kbps"), 210);
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(0.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer app1n3 = onoff1.Install(nodes.Get(3));

    PacketSinkHelper sink1("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    ApplicationContainer app1sink = sink1.Install(nodes.Get(1));
    app1sink.Start(Seconds(0.0));
    app1sink.Stop(Seconds(10.0));

    // FTP over TCP (BulkSend) from n0 to n3
    BulkSendHelper ftp("ns3::TcpSocketFactory", Address(InetSocketAddress(if23.GetAddress(1), port + 1)));
    ftp.SetAttribute("MaxBytes", UintegerValue(0));
    ftp.SetAttribute("StartTime", TimeValue(Seconds(1.2)));
    ftp.SetAttribute("StopTime", TimeValue(Seconds(1.35)));
    ApplicationContainer ftpApp = ftp.Install(nodes.Get(0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port + 1)));
    ApplicationContainer ftpSink = sink2.Install(nodes.Get(3));
    ftpSink.Start(Seconds(1.2));
    ftpSink.Stop(Seconds(1.35));

    AsciiTraceHelper ascii;
    p2p02.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p12.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p23.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}