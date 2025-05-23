#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("Ipv4L3Protocol", ns3::LOG_LEVEL_INFO);

    ns3::NodeContainer nodes;
    nodes.Create(4);

    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer d0d1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    ns3::NetDeviceContainer d1d2 = p2p.Install(nodes.Get(1), nodes.Get(2));
    ns3::NetDeviceContainer d2d3 = p2p.Install(nodes.Get(2), nodes.Get(3));

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    ns3::UdpEchoClientHelper echoClient(i2i3.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    ns3::Simulator::Stop(ns3::Seconds(11.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}