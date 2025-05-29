#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ripng-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngTopologyExample");

int main (int argc, char **argv)
{
    // Enable logging if needed
    // LogComponentEnable ("RipngTopologyExample", LOG_LEVEL_INFO);

    // Split Horizon options: 0 = no split, 1 = split + poison reverse
    RipNg::SplitHorizonType splitHorizon = RipNg::POISON_REVERSE;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6); // 0: SRC, 1: A, 2: B, 3: C, 4: D, 5: DST
    Ptr<Node> SRC = nodes.Get(0);
    Ptr<Node> A = nodes.Get(1);
    Ptr<Node> B = nodes.Get(2);
    Ptr<Node> C = nodes.Get(3);
    Ptr<Node> D = nodes.Get(4);
    Ptr<Node> DST = nodes.Get(5);

    // Point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create NetDevice containers for each link
    NetDeviceContainer d_SRC_A = p2p.Install(SRC, A);    // link 0
    NetDeviceContainer d_A_B   = p2p.Install(A, B);      // link 1
    NetDeviceContainer d_A_C   = p2p.Install(A, C);      // link 2
    NetDeviceContainer d_B_C   = p2p.Install(B, C);      // link 3
    NetDeviceContainer d_B_D   = p2p.Install(B, D);      // link 4
    PointToPointHelper p2pCD; // link C-D has a higher cost
    p2pCD.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pCD.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer d_C_D = p2pCD.Install(C, D);      // link 5 (cost=10)
    NetDeviceContainer d_D_DST = p2p.Install(D, DST);    // link 6

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_SRC_A = ipv6.Assign(d_SRC_A);
    ipv6.NewNetwork();
    Ipv6InterfaceContainer i_A_B   = ipv6.Assign(d_A_B);
    ipv6.NewNetwork();
    Ipv6InterfaceContainer i_A_C   = ipv6.Assign(d_A_C);
    ipv6.NewNetwork();
    Ipv6InterfaceContainer i_B_C   = ipv6.Assign(d_B_C);
    ipv6.NewNetwork();
    Ipv6InterfaceContainer i_B_D   = ipv6.Assign(d_B_D);
    ipv6.NewNetwork();
    Ipv6InterfaceContainer i_C_D   = ipv6.Assign(d_C_D);
    ipv6.NewNetwork();
    Ipv6InterfaceContainer i_D_DST = ipv6.Assign(d_D_DST);

    // Set all interfaces to be up and remove default link-local addresses
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = nodes.Get(i)->GetObject<Ipv6>();
        for (uint32_t j = 0; j < ipv6->GetNInterfaces(); ++j)
        {
            ipv6->SetForwarding(j, true);
            ipv6->SetDefaultRoute(j, 0);
        }
    }

    // RIPng Setup
    RipNgHelper ripng;
    ripng.Set ("SplitHorizon", EnumValue(splitHorizon));

    // Interfaces not participating in RIPng (like host access)
    std::vector<uint32_t> nonRipInterfaces[6];

    nonRipInterfaces[0].push_back(1); // SRC: Only one interface, not running ripng
    nonRipInterfaces[5].push_back(0); // DST: Only one interface, not running ripng

    // Node: A, interfaces: 0(SRC-A), 1(A-B), 2(A-C)
    // Node: B, interfaces: 0(A-B), 1(B-C), 2(B-D)
    // Node: C, interfaces: 0(A-C), 1(B-C), 2(C-D)
    // Node: D, interfaces: 0(B-D), 1(C-D), 2(D-DST)

    // Configure nodes for RIPng
    // Routers: A (1), B (2), C (3), D (4)
    NodeContainer routers;
    routers.Add(A);
    routers.Add(B);
    routers.Add(C);
    routers.Add(D);

    // Tell RIPng which interfaces to exclude (host-facing interfaces)
    ripng.ExcludeInterface(A, 0); // A's interface 0 is to SRC
    ripng.ExcludeInterface(D, 2); // D's interface 2 is to DST

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(SRC);
    internetv6.Install(DST);

    ripng.Install(routers);

    // Assign static addresses for A and D on those interfaces
    Ptr<Ipv6> ipv6A = A->GetObject<Ipv6>();
    ipv6A->SetAddress(0, 1, Ipv6Address("2001:1::A")); // SRC-A link
    ipv6A->SetForwarding(0, true);

    Ptr<Ipv6> ipv6D = D->GetObject<Ipv6>();
    ipv6D->SetAddress(2, 1, Ipv6Address("2001:7::D")); // D-DST link
    ipv6D->SetForwarding(2, true);

    // Enable global routing on hosts to allow ping6 to work
    Ipv6StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv6StaticRouting> staticRoutingSRC = staticRoutingHelper.GetStaticRouting(SRC->GetObject<Ipv6>());
    staticRoutingSRC->AddNetworkRouteTo(Ipv6Address("2001:7::"), Ipv6Prefix(64), i_SRC_A.GetAddress(1, 1), 1);

    Ptr<Ipv6StaticRouting> staticRoutingDST = staticRoutingHelper.GetStaticRouting(DST->GetObject<Ipv6>());
    staticRoutingDST->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), i_D_DST.GetAddress(0, 1), 1);

    // Set link cost between C-D to 10 (default is 1)
    Ptr<NetDevice> devC = d_C_D.Get(0); // C's device connected to D
    Ptr<NetDevice> devD = d_C_D.Get(1); // D's device connected to C
    Ptr<Channel> chCD = devC->GetChannel();
    // Set channel weight property for the high cost (RIPng cost override)
    Ptr<PointToPointChannel> p2pCh = DynamicCast<PointToPointChannel>(chCD);
    if (p2pCh)
    {
        p2pCh->SetAttribute("LinkCost", UintegerValue(10));
    }

    // Generate ICMPv6 Echo Requests (Ping) from SRC to DST after 3s
    uint32_t packetSize = 56;
    uint32_t maxPackets = 10;
    Time interPacketInterval = Seconds(1.0);

    V6PingHelper pingHelper(i_D_DST.GetAddress(1, 1));
    pingHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    pingHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
    pingHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer pingApps = pingHelper.Install(SRC);
    pingApps.Start(Seconds(3.0));
    pingApps.Stop(Seconds(60.0));

    // At 40s, break link B-D (destroy both NetDevices)
    Simulator::Schedule(Seconds(40.0), [&](){
        d_B_D.Get(0)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        d_B_D.Get(1)->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        d_B_D.Get(0)->SetLinkChangeCallback(MakeNullCallback<void>());
        d_B_D.Get(1)->SetLinkChangeCallback(MakeNullCallback<void>());
        d_B_D.Get(0)->SetIfUp(false);
        d_B_D.Get(1)->SetIfUp(false);
    });

    // Recovery at 44s (bring NetDevices back up)
    Simulator::Schedule(Seconds(44.0), [&](){
        d_B_D.Get(0)->SetIfUp(true);
        d_B_D.Get(1)->SetIfUp(true);
        d_B_D.Get(0)->SetReceiveCallback(MakeCallback(&PointToPointNetDevice::Receive, DynamicCast<PointToPointNetDevice>(d_B_D.Get(0))));
        d_B_D.Get(1)->SetReceiveCallback(MakeCallback(&PointToPointNetDevice::Receive, DynamicCast<PointToPointNetDevice>(d_B_D.Get(1))));
    });

    // ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    p2p.EnablePcapAll("ripng-topology", true);
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng-topology.tr"));

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}