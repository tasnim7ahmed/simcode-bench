#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6FragmentationSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodesSrc;
    nodesSrc.Create(1); // Src (n0)
    NodeContainer router;
    router.Create(1);   // r
    NodeContainer nodesDst;
    nodesDst.Create(1); // Dst (n1)

    NodeContainer p2pNodes0 = NodeContainer(nodesSrc.Get(0), router.Get(0));
    NodeContainer p2pNodes1 = NodeContainer(router.Get(0), nodesDst.Get(0));

    PointToPointHelper p2p0;
    p2p0.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p0.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p0.SetDeviceAttribute("Mtu", UintegerValue(5000));

    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p1.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices0 = p2p0.Install(p2pNodes0);
    NetDeviceContainer devices1 = p2p1.Install(p2pNodes1);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv6AddressHelper address;
    Ipv6InterfaceContainer interfaces0;
    Ipv6InterfaceContainer interfaces1;

    address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    interfaces0 = address.Assign(devices0);
    interfaces0.SetForwarding(0, true);
    interfaces0.SetDefaultRouteInAllNodes(0);
    interfaces0.SetForwarding(1, true);
    interfaces0.SetDefaultRouteInAllNodes(1);

    address.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    interfaces1 = address.Assign(devices1);
    interfaces1.SetForwarding(0, true);
    interfaces1.SetDefaultRouteInAllNodes(0);
    interfaces1.SetForwarding(1, true);
    interfaces1.SetDefaultRouteInAllNodes(1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodesDst.Get(0));
    serverApps.Start(Seconds(1.0));
    serverAds.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces1.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000));

    ApplicationContainer clientApps = echoClient.Install(nodesSrc.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p0.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-two-mtu.tr"));
    p2p1.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-two-mtu.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}