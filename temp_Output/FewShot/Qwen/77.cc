#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging
    LogComponentEnable("RipNg", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    // Create nodes
    Ptr<Node> src = CreateObject<Node>();
    Ptr<Node> a = CreateObject<Node>();
    Ptr<Node> b = CreateObject<Node>();
    Ptr<Node> c = CreateObject<Node>();
    Ptr<Node> d = CreateObject<Node>();
    Ptr<Node> dst = CreateObject<Node>();

    NodeContainer allNodes(src, a, b, c, d, dst);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Links: SRC <-> A
    NetDeviceContainer devSrcA = p2p.Install(NodeContainer(src, a));

    // Links: A <-> B
    NetDeviceContainer devAB = p2p.Install(NodeContainer(a, b));

    // Links: A <-> C
    NetDeviceContainer devAC = p2p.Install(NodeContainer(a, c));

    // Links: B <-> C
    NetDeviceContainer devBC = p2p.Install(NodeContainer(b, c));

    // Links: B <-> D (cost 1)
    NetDeviceContainer devBD = p2p.Install(NodeContainer(b, d));

    // Links: C <-> D (cost 10)
    NetDeviceContainer devCD = p2p.Install(NodeContainer(c, d));
    p2p.SetChannelAttribute("Delay", StringValue("10ms")); // higher delay to match cost
    NetDeviceContainer devCD_highCost = devCD; // Reuse same devices

    // Links: D <-> DST
    NetDeviceContainer devDDst = p2p.Install(NodeContainer(d, dst));

    // Install Internet stack with RIPng
    InternetStackHelper internet;
    RipNgHelper ripNgRouting;

    // Enable Split Horizon
    ripNgRouting.SetInterfaceAttribute("SplitHorizon", EnumValue(RipNg::SPLIT_HORIZON));

    internet.SetRoutingHelper(ripNgRouting); // has effect on the next Install()
    internet.Install(allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iSrcA = ipv6.Assign(devSrcA);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iAB = ipv6.Assign(devAB);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iAC = ipv6.Assign(devAC);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iBC = ipv6.Assign(devBC);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iBD = ipv6.Assign(devBD);

    ipv6.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iCD = ipv6.Assign(devCD);

    ipv6.SetBase(Ipv6Address("2001:7::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iDDst = ipv6.Assign(devDDst);

    // Manually set link costs for specific interfaces
    // C <-> D link cost is 10
    for (uint32_t i = 0; i < devCD.GetN(); ++i) {
        devCD.Get(i)->SetAttribute("DataRate", DataRateValue(DataRate("1Kbps"))); // low rate to simulate high cost
    }

    // Ping application from SRC to DST
    V4PingHelper ping(dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ApplicationContainer clientApp = ping.Install(src);
    clientApp.Start(Seconds(3.0));
    clientApp.Stop(Seconds(60.0));

    // Schedule failure and recovery of BD link
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkStatus, DynamicCast<PointToPointNetDevice>(devBD.Get(0)), false);
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkStatus, DynamicCast<PointToPointNetDevice>(devBD.Get(0)), true);

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng_topology.tr"));

    // Enable pcap tracing
    p2p.EnablePcapAll("ripng_topology");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}