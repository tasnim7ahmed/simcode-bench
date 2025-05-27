#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/radvd-module.h"
#include "ns3/icmpv6-l4-protocol.h"
#include "ns3/ping6-application.h"
#include "ns3/net-device.h"
#include "ns3/queue.h"
#include "ns3/queue-disc.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefix");

int main(int argc, char* argv[]) {
    LogComponentEnable("RadvdTwoPrefix", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    CsmaHelper csma0;
    csma0.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma0.SetChannelAttribute("Delay", TimeValue(MicroSeconds(5)));

    NetDeviceContainer devices0 = csma0.Install(NodeContainer(nodes.Get(0), router.Get(0)));

    CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma1.SetChannelAttribute("Delay", TimeValue(MicroSeconds(5)));

    NetDeviceContainer devices1 = csma1.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    InternetStackHelper internetv6;
    internetv6.Install(nodes);
    internetv6.Install(router);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = ipv6.Assign(devices0);
    interfaces0.SetForwarding(1, true);
    interfaces0.SetDefaultRouteInAllNodes(0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = ipv6.Assign(devices1);
    interfaces1.SetForwarding(1, true);
    interfaces1.SetDefaultRouteInAllNodes(0);

    Ipv6Address addr1_1("2001:1::1");
    Ipv6Address addr_abcd("2001:abcd::1");
    interfaces0.GetAddress(1, 0);
    interfaces0.AddAddress(1, addr1_1);
    interfaces0.AddAddress(1, addr_abcd);
    interfaces0.SetRouter(1, true);

    Ipv6Address addr2_1("2001:2::1");
    interfaces1.GetAddress(1, 0);
    interfaces1.AddAddress(1, addr2_1);
    interfaces1.SetRouter(1, true);

    RadvdHelper radvd0;
    radvd0.SetPrefix("2001:1::/64", true);
    radvd0.SetPrefix("2001:abcd::/64", true);
    radvd0.Install(router.Get(0), devices0.Get(1));

    RadvdHelper radvd1;
    radvd1.SetPrefix("2001:2::/64", true);
    radvd1.Install(router.Get(0), devices1.Get(1));

    Ping6ApplicationHelper ping6;
    ping6.SetRemote(interfaces1.GetAddress(0, 0));
    ping6.SetAttribute("Verbose", BooleanValue(true));

    ApplicationContainer p = ping6.Install(nodes.Get(0));
    p.Start(Seconds(1.0));
    p.Stop(Seconds(10.0));

    for (uint32_t i = 0; i < devices0.GetN(); ++i) {
        devices0.Get(i)->TraceConnectWithoutContext("PhyTxQueue/Size", "/ChannelQueue/Size");
    }
    for (uint32_t i = 0; i < devices1.GetN(); ++i) {
        devices1.Get(i)->TraceConnectWithoutContext("PhyTxQueue/Size", "/ChannelQueue/Size");
    }
    devices0.Get(0)->TraceConnectWithoutContext("PhyRx", "/PacketSink/Rx");
    devices1.Get(0)->TraceConnectWithoutContext("PhyRx", "/PacketSink/Rx");

    Simulator::Stop(Seconds(11.0));
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("radvd-two-prefix.tr");
    csma0.EnableAsciiAll(stream);
    csma1.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}