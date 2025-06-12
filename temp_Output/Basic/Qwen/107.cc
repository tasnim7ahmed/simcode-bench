#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6TwoMtu");

int main(int argc, char *argv[]) {
    bool verbose = true;
    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodesSrc;
    nodesSrc.Create(1); // Src
    NodeContainer nodesDst;
    nodesDst.Create(1); // Dst

    NodeContainer n0r;
    n0r.Create(2); // n0 and r
    NodeContainer r1n1;
    r1n1.Add(n0r.Get(1)); // r
    r1n1.Create(1); // n1

    NodeContainer allNodes = NodeContainer(nodesSrc, n0r, r1n1, nodesDst);

    CsmaHelper csma0;
    csma0.SetChannelAttribute("DataRate", DataRateValue(DataRate(5Mbps)));
    csma0.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma0.SetDeviceAttribute("Mtu", UintegerValue(5000));

    CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", DataRateValue(DataRate(5Mbps)));
    csma1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma1.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices0 = csma0.Install(NodeContainer(nodesSrc, n0r.Get(0)));
    NetDeviceContainer devices1 = csma1.Install(NodeContainer(r1n1.Get(0), nodesDst));

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = address.Assign(devices0);
    interfaces0.SetForwarding(1, true);
    interfaces0.SetDefaultRouteInAllNodes();

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = address.Assign(devices1);
    interfaces1.SetForwarding(0, true);
    interfaces1.SetDefaultRouteInAllNodes();

    // Set up static routing between the two routers (if needed)
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> r1Routing = routingHelper.GetStaticRouting(nodesDst.Get(0)->GetObject<Ipv6>());
    r1Routing->AddNetworkRouteTo(Ipv6Address("2001:1::0"), Ipv6Prefix(64), 1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodesDst.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces1.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000));

    ApplicationContainer clientApps = echoClient.Install(nodesSrc.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiHelper;
    Ptr<OutputStreamWrapper> stream = asciiHelper.CreateFileStream("fragmentation-ipv6-two-mtu.tr");
    csma0.EnableAsciiAll(stream);
    csma1.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}