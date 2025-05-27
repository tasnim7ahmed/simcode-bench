#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/olsr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(5);

    InternetStackHelper internet;
    internet.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices.Add(p2p.Install(nodes.Get(0), nodes.Get(1)));
    devices.Add(p2p.Install(nodes.Get(0), nodes.Get(2)));
    devices.Add(p2p.Install(nodes.Get(0), nodes.Get(3)));
    devices.Add(p2p.Install(nodes.Get(0), nodes.Get(4)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Multicast setup
    Ipv4Address multicastAddress("225.1.2.3");
    Ptr<Ipv4> ipv4_0 = nodes.Get(0)->GetObject<Ipv4>();
    ipv4_0->SetDefaultMulticastForwarding(true);
    for (int i = 1; i < 5; ++i) {
        Ptr<Ipv4> ipv4_i = nodes.Get(i)->GetObject<Ipv4>();
        ipv4_i->SetDefaultMulticastForwarding(true);
        ipv4_i->AddMulticastRoute(multicastAddress, 0);
        ipv4_i->JoinGroup(interfaces.GetAddress(i - 1), multicastAddress);
    }

    // Blacklisting links
    Ipv4GlobalRoutingHelper::BlacklistRoute(nodes.Get(0)->GetId(), interfaces.GetAddress(0), interfaces.GetAddress(1));

    // UDP multicast traffic
    uint16_t multicastPort = 9;
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(multicastAddress, multicastPort)));
    onOffHelper.SetConstantRate(DataRate("1Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer app = onOffHelper.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    // Packet sinks on nodes B, C, D, E
    for (int i = 1; i < 5; ++i) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(i - 1), multicastPort));
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));
    }

    // Pcap Tracing
    p2p.EnablePcapAll("multicast");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}