#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimulationScript");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2p2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer devices02 = p2p1.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices12 = p2p1.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = p2p2.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if02 = address.Assign(devices02);

    address.NewNetwork();
    Ipv4InterfaceContainer if12 = address.Assign(devices12);

    address.NewNetwork();
    Ipv4InterfaceContainer if23 = address.Assign(devices23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    // CBR/UDP flow from n0 to n3
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(2.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(if23.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("448kbps"), 210);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(2.0)));

    ApplicationContainer app0 = onoff.Install(nodes.Get(0));
    app0.Start(Seconds(0.5));
    app0.Stop(Seconds(2.0));

    // CBR/UDP flow from n3 to n1
    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer sinkApp2 = sink2.Install(nodes.Get(1));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(2.0));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(if12.GetAddress(1), port + 1));
    onoff2.SetConstantRate(DataRate("448kbps"), 210);
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(0.7)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(2.0)));

    ApplicationContainer app3 = onoff2.Install(nodes.Get(3));
    app3.Start(Seconds(0.7));
    app3.Stop(Seconds(2.0));

    // FTP/TCP flow from n0 to n3 (active between 1.2s and 1.35s)
    PacketSinkHelper sink3("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 2));
    ApplicationContainer sinkApp3 = sink3.Install(nodes.Get(3));
    sinkApp3.Start(Seconds(1.0));
    sinkApp3.Stop(Seconds(1.5));

    BulkSendHelper ftp("ns3::TcpSocketFactory", InetSocketAddress(if23.GetAddress(1), port + 2));
    ftp.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited

    ApplicationContainer appFtp = ftp.Install(nodes.Get(0));
    appFtp.Start(Seconds(1.2));
    appFtp.Stop(Seconds(1.35));

    AsciiTraceHelper ascii;
    p2p1.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));
    p2p2.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}