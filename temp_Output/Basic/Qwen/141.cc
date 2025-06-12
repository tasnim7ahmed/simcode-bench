#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("RingTopologySimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[3];
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);

    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinks[3];
    for (uint32_t i = 0; i < 3; ++i) {
        sinks[i] = sink.Install(nodes.Get(i));
        sinks[i].Start(Seconds(0.0));
        sinks[i].Stop(Seconds(10.0));
    }

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    for (uint32_t i = 0; i < 3; ++i) {
        uint32_t j = (i + 1) % 3;
        Address remoteAddress(InetSocketAddress(interfaces[j].GetAddress(1), port));
        onoff.SetAttribute("Remote", AddressValue(remoteAddress));
        ApplicationContainer apps = onoff.Install(nodes.Get(i));
        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(9.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}