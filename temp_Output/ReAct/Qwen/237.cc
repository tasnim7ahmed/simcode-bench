#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpRelaySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper client(interfaces.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();

    uint32_t netdevA = ipv4A->GetInterfaceForAddress(interfaces.GetAddress(0));
    uint32_t netdevC = ipv4C->GetInterfaceForAddress(interfaces.GetAddress(2));

    if (netdevA != 0xffffffff) {
        ipv4A->SetDown(netdevA);
    }
    if (netdevC != 0xffffffff) {
        ipv4C->SetDown(netdevC);
    }

    Ipv4StaticRouting* routeA = routingHelper.GetStaticRouting(ipv4A);
    routeA->AddHostRouteTo(Ipv4Address("10.1.1.2"), Ipv4Address("10.1.1.3"), 2);

    Ipv4StaticRouting* routeC = routingHelper.GetStaticRouting(ipv4C);
    routeC->AddHostRouteTo(Ipv4Address("10.1.1.2"), 2);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}