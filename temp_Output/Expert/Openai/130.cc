#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-nat-helper.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer privateHosts;
    privateHosts.Create(2);

    NodeContainer natRouter;
    natRouter.Create(1);

    NodeContainer publicServer;
    publicServer.Create(1);

    NodeContainer privateNetwork;
    privateNetwork.Add(privateHosts);
    privateNetwork.Add(natRouter);

    NodeContainer publicNetwork;
    publicNetwork.Add(natRouter.Get(0));
    publicNetwork.Add(publicServer.Get(0));

    CsmaHelper csmaPrivate;
    csmaPrivate.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaPrivate.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    CsmaHelper csmaPublic;
    csmaPublic.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaPublic.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    NetDeviceContainer privateDevices = csmaPrivate.Install(privateNetwork);
    NetDeviceContainer publicDevices = csmaPublic.Install(publicNetwork);

    InternetStackHelper stack;
    stack.Install(privateHosts);
    stack.Install(publicServer);

    // For NAT router, do not add IPv4 stack now.
    Ipv4NATHelper natHelper;
    natHelper.SetAttribute("InsideInterface", UintegerValue(1)); // To be set after stack

    InternetStackHelper natStack;
    natStack.Install(natRouter);

    Ipv4AddressHelper addressPrivate;
    addressPrivate.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer privateIfs = addressPrivate.Assign(privateDevices);

    Ipv4AddressHelper addressPublic;
    addressPublic.SetBase("203.0.113.0", "255.255.255.0");
    Ipv4InterfaceContainer publicIfs = addressPublic.Assign(publicDevices);

    // Assign NAT on router node.
    Ptr<Node> natNode = natRouter.Get(0);

    Ptr<Ipv4> ipv4 = natNode->GetObject<Ipv4>();
    // Interface 1 = private, Interface 2 = public (per containers used)
    uint32_t privateIfIdx = 1;
    uint32_t publicIfIdx = 2;

    natHelper.SetAttribute("InsideInterface", UintegerValue(privateIfIdx));
    natHelper.SetAttribute("OutsideInterface", UintegerValue(publicIfIdx));
    natHelper.Install(natNode);

    // Enable NAT on router
    Ptr<Ipv4Nat> nat = natNode->GetObject<Ipv4Nat>();

    // Static routing for both sides
    Ipv4StaticRoutingHelper staticRoutingHelper;
    for (uint32_t i = 0; i < privateHosts.GetN(); ++i) {
        Ptr<Ipv4> hostIpv4 = privateHosts.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> staticRt = staticRoutingHelper.GetStaticRouting(hostIpv4);
        staticRt->SetDefaultRoute(privateIfs.GetAddress(natRouter.GetN(), 0), 1);
    }
    Ptr<Ipv4StaticRouting> serverRt = staticRoutingHelper.GetStaticRouting(
        publicServer.Get(0)->GetObject<Ipv4>());
    serverRt->SetDefaultRoute(publicIfs.GetAddress(0, 0), 1);

    // NAT router needs to route between subnets
    Ptr<Ipv4StaticRouting> natStaticRt = staticRoutingHelper.GetStaticRouting(ipv4);
    natStaticRt->AddNetworkRouteTo(Ipv4Address("10.0.0.0"), Ipv4Mask("255.255.255.0"), privateIfIdx);
    natStaticRt->AddNetworkRouteTo(Ipv4Address("203.0.113.0"), Ipv4Mask("255.255.255.0"), publicIfIdx);

    // UDP Echo Server on public server
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(publicServer.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(8.0));

    // UDP Echo Clients on private hosts, target public server's IP
    for (uint32_t i = 0; i < privateHosts.GetN(); ++i) {
        UdpEchoClientHelper echoClient(publicIfs.GetAddress(1), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(3));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApps = echoClient.Install(privateHosts.Get(i));
        clientApps.Start(Seconds(2.0 + i));
        clientApps.Stop(Seconds(7.0));
    }

    // Enable pcap for packet tracing
    csmaPrivate.EnablePcap("nat-private", privateDevices, true);
    csmaPublic.EnablePcap("nat-public", publicDevices, true);

    Simulator::Stop(Seconds(8.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}