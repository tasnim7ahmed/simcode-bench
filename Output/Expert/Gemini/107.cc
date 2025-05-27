#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/packet-socket-factory.h"
#include "ns3/ipv6-extension-header.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(5);

    NodeContainer n0r = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer n1r = NodeContainer(nodes.Get(3), nodes.Get(2));
    NodeContainer srn0 = NodeContainer(nodes.Get(0),nodes.Get(1));
    NodeContainer n1dst = NodeContainer(nodes.Get(3),nodes.Get(4));

    CsmaHelper csma0r;
    csma0r.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma0r.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma0r.SetDeviceAttribute("Mtu", UintegerValue(5000));

    CsmaHelper csma1r;
    csma1r.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma1r.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma1r.SetDeviceAttribute("Mtu", UintegerValue(1500));

    CsmaHelper csmasrn0;
    csmasrn0.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmasrn0.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    CsmaHelper csman1dst;
    csman1dst.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csman1dst.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer d0r = csma0r.Install(n0r);
    NetDeviceContainer d1r = csma1r.Install(n1r);
    NetDeviceContainer dsrn0 = csmasrn0.Install(srn0);
    NetDeviceContainer dn1dst = csman1dst.Install(n1dst);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i0r = ipv6.Assign(d0r);
    ipv6.SetBase(Ipv6Address("2001:db8:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1r = ipv6.Assign(d1r);
    ipv6.SetBase(Ipv6Address("2001:db8:0:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer isrn0 = ipv6.Assign(dsrn0);
    ipv6.SetBase(Ipv6Address("2001:db8:0:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer in1dst = ipv6.Assign(dn1dst);

    Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get(2)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(i1r.GetAddress(1), 0);
    staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get(2)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(i0r.GetAddress(1), 0);

    staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:0:4::2"),i0r.GetAddress(1),0);
    staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get(3)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:0:3::1"),i1r.GetAddress(1),0);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i1r.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(4000));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    csma0r.EnablePcap("fragmentation-ipv6-two-mtu", d0r.Get(0), true);
    csma1r.EnablePcap("fragmentation-ipv6-two-mtu", d1r.Get(0), true);
    csmasrn0.EnablePcap("fragmentation-ipv6-two-mtu", dsrn0.Get(0),true);
    csman1dst.EnablePcap("fragmentation-ipv6-two-mtu", dn1dst.Get(0),true);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}