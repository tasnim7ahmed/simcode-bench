#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging (optional)
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes: Four routers
    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links between routers to form a mesh topology
    // r0--r1--r2--r3, plus r0--r2 and r1--r3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links
    NodeContainer n0n1 = NodeContainer(routers.Get(0), routers.Get(1));
    NodeContainer n1n2 = NodeContainer(routers.Get(1), routers.Get(2));
    NodeContainer n2n3 = NodeContainer(routers.Get(2), routers.Get(3));
    NodeContainer n0n2 = NodeContainer(routers.Get(0), routers.Get(2));
    NodeContainer n1n3 = NodeContainer(routers.Get(1), routers.Get(3));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d3 = p2p.Install(n1n3);

    // Install OSPF on all routers
    OspfRoutingHelper ospfRouting;
    InternetStackHelper internet;
    internet.SetRoutingHelper(ospfRouting);
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer i0i1, i1i2, i2i3, i0i2, i1i3;

    address.SetBase("10.0.1.0", "255.255.255.0");
    i0i1 = address.Assign(d0d1);

    address.SetBase("10.0.2.0", "255.255.255.0");
    i1i2 = address.Assign(d1d2);

    address.SetBase("10.0.3.0", "255.255.255.0");
    i2i3 = address.Assign(d2d3);

    address.SetBase("10.0.4.0", "255.255.255.0");
    i0i2 = address.Assign(d0d2);

    address.SetBase("10.0.5.0", "255.255.255.0");
    i1i3 = address.Assign(d1d3);

    // Assign loopback addresses so applications can bind to them
    Ipv4InterfaceContainer loopbacks;
    for (uint32_t i = 0; i < routers.GetN(); ++i)
    {
        Ptr<Node> node = routers.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        int32_t loif = ipv4->GetInterfaceForAddress(Ipv4Address("127.0.0.1"), Ipv4Mask("255.0.0.0"));
        if (loif == -1)
        {
            loif = ipv4->AddInterface(CreateObject<LoopbackNetDevice>());
            ipv4->AddAddress(loif, Ipv4InterfaceAddress("127.0.0.1", "255.0.0.0"));
            ipv4->SetMetric(loif, 1);
            ipv4->SetUp(loif);
        }
    }

    // Install a UDP server on router 3
    uint16_t port = 4000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(routers.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Install a UDP client on router 0, send to router 3's address on the last link
    UdpEchoClientHelper echoClient(i2i3.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(routers.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Enable pcap tracing on all devices to observe both control and data traffic
    p2p.EnablePcapAll("ospf-ls-network");

    // Run simulation
    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}