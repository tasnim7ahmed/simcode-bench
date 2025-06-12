#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpP2PExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientRouter = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer routerServer = p2p.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientIf = address.Assign(clientRouter);

    address.NewNetwork();
    Ipv4InterfaceContainer serverIf = address.Assign(routerServer);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;
    Address serverAddress(InetSocketAddress(serverIf.GetAddress(1), port));

    OnOffHelper onoff("ns3::TcpSocketFactory", serverAddress);
    onoff.SetConstantRate(DataRate("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(11.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}