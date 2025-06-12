#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-generator.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ripng-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipNgExample");

int
main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("RipNg", LOG_LEVEL_INFO);
    LogComponentEnable("V6Ping", LOG_LEVEL_INFO);

    // Split Horizon option: 0 = No split, 1 = Split Horizon, 2 = Split Horizon with Poison Reverse
    uint32_t splitHorizon = 1;

    // Create nodes
    Ptr<Node> SRC = CreateObject<Node>();
    Ptr<Node> A   = CreateObject<Node>();
    Ptr<Node> B   = CreateObject<Node>();
    Ptr<Node> C   = CreateObject<Node>();
    Ptr<Node> D   = CreateObject<Node>();
    Ptr<Node> DST = CreateObject<Node>();

    Names::Add("SRC", SRC);
    Names::Add("A", A);
    Names::Add("B", B);
    Names::Add("C", C);
    Names::Add("D", D);
    Names::Add("DST", DST);

    NodeContainer nodes;
    nodes.Add(SRC);
    nodes.Add(A);
    nodes.Add(B);
    nodes.Add(C);
    nodes.Add(D);
    nodes.Add(DST);

    // Create links as per topology
    NetDeviceContainer dSRC_A = PointToPointHelper().SetDeviceAttribute("DataRate", StringValue("10Mbps"))
                                                    .SetChannelAttribute("Delay", StringValue("2ms"))
                                                    .Install(SRC, A);

    NetDeviceContainer dA_B = PointToPointHelper().SetDeviceAttribute("DataRate", StringValue("10Mbps"))
                                                  .SetChannelAttribute("Delay", StringValue("2ms"))
                                                  .Install(A, B);

    NetDeviceContainer dA_C = PointToPointHelper().SetDeviceAttribute("DataRate", StringValue("10Mbps"))
                                                  .SetChannelAttribute("Delay", StringValue("2ms"))
                                                  .Install(A, C);

    NetDeviceContainer dB_C = PointToPointHelper().SetDeviceAttribute("DataRate", StringValue("10Mbps"))
                                                  .SetChannelAttribute("Delay", StringValue("2ms"))
                                                  .Install(B, C);

    NetDeviceContainer dB_D = PointToPointHelper().SetDeviceAttribute("DataRate", StringValue("10Mbps"))
                                                  .SetChannelAttribute("Delay", StringValue("2ms"))
                                                  .Install(B, D);

    NetDeviceContainer dC_D = PointToPointHelper().SetDeviceAttribute("DataRate", StringValue("10Mbps"))
                                                  .SetChannelAttribute("Delay", StringValue("2ms"))
                                                  .Install(C, D);

    NetDeviceContainer dD_DST = PointToPointHelper().SetDeviceAttribute("DataRate", StringValue("10Mbps"))
                                                    .SetChannelAttribute("Delay", StringValue("2ms"))
                                                    .Install(D, DST);

    // For C-D link, manually set channel metric to 10 via interface metric

    // Install IPv6 stack
    InternetStackHelper internetv6;
    RipNgHelper ripNg;

    // Configure Split Horizon
    if (splitHorizon == 0)
        ripNg.Set("SplitHorizon", EnumValue(RipNgHelper::NO_SPLIT_HORIZON));
    else if (splitHorizon == 1)
        ripNg.Set("SplitHorizon", EnumValue(RipNgHelper::SPLIT_HORIZON));
    else
        ripNg.Set("SplitHorizon", EnumValue(RipNgHelper::POISON_REVERSE));

    // Enable RIPng on routers
    NodeContainer routers;
    routers.Add(A); routers.Add(B); routers.Add(C); routers.Add(D);
    ripNg.ExcludeInterface(A, 1); // We'll set A as static at first so exclude ripng from primary interface
    ripNg.ExcludeInterface(D, 1); // Likewise D

    internetv6.SetRoutingHelper(ripNg);
    internetv6.Install(routers);

    // End nodes: SRC and DST
    internetv6.Install(SRC);
    internetv6.Install(DST);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase("2001:1::", 64);
    Ipv6InterfaceContainer iSRC_A = ipv6.Assign(dSRC_A);

    ipv6.SetBase("2001:2::", 64);
    Ipv6InterfaceContainer iA_B = ipv6.Assign(dA_B);

    ipv6.SetBase("2001:3::", 64);
    Ipv6InterfaceContainer iA_C = ipv6.Assign(dA_C);

    ipv6.SetBase("2001:4::", 64);
    Ipv6InterfaceContainer iB_C = ipv6.Assign(dB_C);

    ipv6.SetBase("2001:5::", 64);
    Ipv6InterfaceContainer iB_D = ipv6.Assign(dB_D);

    ipv6.SetBase("2001:6::", 64);
    Ipv6InterfaceContainer iC_D = ipv6.Assign(dC_D);

    ipv6.SetBase("2001:7::", 64);
    Ipv6InterfaceContainer iD_DST = ipv6.Assign(dD_DST);

    // Ensure all interfaces are up
    iSRC_A.SetForwarding(0, true);
    iSRC_A.SetDefaultRouteInAllNodes (0);

    iA_B.SetForwarding(0, true);
    iA_B.SetForwarding(1, true);

    iA_C.SetForwarding(0, true);
    iA_C.SetForwarding(1, true);

    iB_C.SetForwarding(0, true);
    iB_C.SetForwarding(1, true);

    iB_D.SetForwarding(0, true);
    iB_D.SetForwarding(1, true);

    iC_D.SetForwarding(0, true);
    iC_D.SetForwarding(1, true);

    iD_DST.SetForwarding(0, true);
    iD_DST.SetForwarding(1, true);

    // Set static addresses for A and D for their first interfaces (index 1)
    Ptr<Ipv6> ipv6A = A->GetObject<Ipv6>();
    Ptr<Ipv6> ipv6D = D->GetObject<Ipv6>();
    ipv6A->SetMetric (1, 1);
    ipv6D->SetMetric (1, 1);

    // Set metric for C-D (second device of C, index 3 device interface, depends on installation order)
    Ptr<Ipv6> ipv6C = C->GetObject<Ipv6>();
    // Find the interface corresponding to dC_D
    uint32_t c_c_d_ifIndex = ipv6C->GetInterfaceForDevice(dC_D.Get(0));
    ipv6C->SetMetric(c_c_d_ifIndex, 10);

    // Also set metric 10 on D for D's interface to C
    uint32_t d_c_d_ifIndex = ipv6D->GetInterfaceForDevice(dC_D.Get(1));
    ipv6D->SetMetric(d_c_d_ifIndex, 10);

    // Configure static default routes on SRC and DST
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRoutingSRC = ipv6RoutingHelper.GetStaticRouting(SRC->GetObject<Ipv6>());
    Ptr<Ipv6StaticRouting> staticRoutingDST = ipv6RoutingHelper.GetStaticRouting(DST->GetObject<Ipv6>());

    staticRoutingSRC->SetDefaultRoute(iSRC_A.GetAddress(1, 1), 1);
    staticRoutingDST->SetDefaultRoute(iD_DST.GetAddress(0, 1), 1);

    // Install applications
    uint32_t packetSize = 56;
    uint32_t maxPackets = 1000;
    Time interPacketInterval = Seconds(1.0);

    // V6Ping from SRC to DST
    V6PingHelper ping6(iD_DST.GetAddress(1, 1));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("Size", UintegerValue(packetSize));
    ping6.SetAttribute("Count", UintegerValue(maxPackets));

    ApplicationContainer pingApps = ping6.Install(SRC);
    pingApps.Start(Seconds(3.0));
    pingApps.Stop(Seconds(60.0));

    // Tracing
    AsciiTraceHelper ascii;
    PointToPointHelper p2p;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng-topology.tr"));
    p2p.EnablePcapAll("ripng-topology", true);

    // Bring down the B-D link at 40s
    Simulator::Schedule(Seconds(40.0), [&](){
        NS_LOG_INFO("Bringing down B-D link at 40s");
        Ptr<NetDevice> ndB = dB_D.Get(0);
        Ptr<NetDevice> ndD = dB_D.Get(1);
        ndB->SetLinkDown();
        ndD->SetLinkDown();
    });

    // Restore the B-D link at 44s
    Simulator::Schedule(Seconds(44.0), [&](){
        NS_LOG_INFO("Restoring B-D link at 44s");
        Ptr<NetDevice> ndB = dB_D.Get(0);
        Ptr<NetDevice> ndD = dB_D.Get(1);
        ndB->SetLinkUp();
        ndD->SetLinkUp();
    });

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}