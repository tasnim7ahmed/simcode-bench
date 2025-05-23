#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-module.h"

int main()
{
    ns3::NodeContainer routers;
    routers.Create(3); // R1, R2, R3

    ns3::NodeContainer hosts;
    hosts.Create(3); // H1, H2, H3

    ns3::Ptr<ns3::Node> h1 = hosts.Get(0);
    ns3::Ptr<ns3::Node> h2 = hosts.Get(1);
    ns3::Ptr<ns3::Node> h3 = hosts.Get(2);

    ns3::Ptr<ns3::Node> r1 = routers.Get(0);
    ns3::Ptr<ns3::Node> r2 = routers.Get(1);
    ns3::Ptr<ns3::Node> r3 = routers.Get(2);

    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer r1h1Devices, r2h2Devices, r3h3Devices;
    ns3::NetDeviceContainer r1r2Devices, r2r3Devices, r3r1Devices;

    r1h1Devices = p2p.Install(r1, h1);
    r2h2Devices = p2p.Install(r2, h2);
    r3h3Devices = p2p.Install(r3, h3);

    r1r2Devices = p2p.Install(r1, r2);
    r2r3Devices = p2p.Install(r2, r3);
    r3r1Devices = p2p.Install(r3, r1);

    ns3::InternetStackHelper internet;
    internet.SetRoutingHelper(ns3::RipHelper());
    internet.Install(routers);

    ns3::InternetStackHelper hostInternet;
    hostInternet.Install(hosts);

    ns3::Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer r1h1Ifaces = ipv4.Assign(r1h1Devices);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer r2h2Ifaces = ipv4.Assign(r2h2Devices);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer r3h3Ifaces = ipv4.Assign(r3h3Devices);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer r1r2Ifaces = ipv4.Assign(r1r2Devices);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer r2r3Ifaces = ipv4.Assign(r2r3Devices);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer r3r1Ifaces = ipv4.Assign(r3r1Devices);

    ns3::Ipv4StaticRoutingHelper staticRoutingHelper;
    ns3::Ptr<ns3::Ipv4StaticRouting> h1StaticRouting = staticRoutingHelper.Get(h1->GetObject<ns3::Ipv4>()->GetRouting<ns3::Ipv4StaticRouting>());
    h1StaticRouting->SetDefaultRoute(r1h1Ifaces.GetAddress(0), 1);

    ns3::Ptr<ns3::Ipv4StaticRouting> h2StaticRouting = staticRoutingHelper.Get(h2->GetObject<ns3::Ipv4>()->GetRouting<ns3::Ipv4StaticRouting>());
    h2StaticRouting->SetDefaultRoute(r2h2Ifaces.GetAddress(0), 1);

    ns3::Ptr<ns3::Ipv4StaticRouting> h3StaticRouting = staticRoutingHelper.Get(h3->GetObject<ns3::Ipv4>()->GetRouting<ns3::Ipv4StaticRouting>());
    h3StaticRouting->SetDefaultRoute(r3h3Ifaces.GetAddress(0), 1);

    ns3::UdpEchoServerHelper echoServer(9);
    ns3::ApplicationContainer serverApps = echoServer.Install(h3);
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(20.0));

    ns3::UdpEchoClientHelper echoClient(r3h3Ifaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(h1);
    clientApps.Start(ns3::Seconds(5.0));
    clientApps.Stop(ns3::Seconds(15.0));

    p2p.EnablePcapAll("rip-dvr-simulation");

    ns3::Simulator::Stop(ns3::Seconds(25.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}