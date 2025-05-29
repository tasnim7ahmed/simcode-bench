#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d34 = p2p.Install(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer d45 = p2p.Install(nodes.Get(4), nodes.Get(5));

    InternetStackHelper stack;
    for (uint32_t i = 0; i < 6; ++i) {
        stack.Install(nodes.Get(i));
    }

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer i01, i12, i23, i34, i45;

    address.SetBase("10.1.1.0", "255.255.255.0");
    i01 = address.Assign(d01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    i12 = address.Assign(d12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    i23 = address.Assign(d23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    i34 = address.Assign(d34);

    address.SetBase("10.1.5.0", "255.255.255.0");
    i45 = address.Assign(d45);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i45.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}