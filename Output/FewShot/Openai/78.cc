#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipPoisonReverseExample");

void
PrintRoutingTable(Ptr<Node> node, Time printTime)
{
    Ipv4RoutingHelper::PrintRoutingTableAllAt(printTime, OutputStreamWrapper(&std::cout));
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    bool printRoutingTables = false;
    bool showPings = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable extra log components", verbose);
    cmd.AddValue("printRoutingTables", "Print routing tables at regular intervals", printRoutingTables);
    cmd.AddValue("showPings", "Show Ping6 reception", showPings);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("RipSimpleRouting", LOG_LEVEL_INFO);
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4StaticRouting", LOG_LEVEL_ALL);
        LogComponentEnable("PingHelper", LOG_LEVEL_INFO);
    }

    // Nodes: SRC, A, B, C, D, DST
    NodeContainer nodes;
    nodes.Create(6);
    Ptr<Node> src = nodes.Get(0);
    Ptr<Node> a = nodes.Get(1);
    Ptr<Node> b = nodes.Get(2);
    Ptr<Node> c = nodes.Get(3);
    Ptr<Node> d = nodes.Get(4);
    Ptr<Node> dst = nodes.Get(5);

    // Point-to-Point Helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // NetDevice and Ipv4Interface containers for each link
    NetDeviceContainer dev_s_a = p2p.Install(src, a);
    NetDeviceContainer dev_a_b = p2p.Install(a, b);
    NetDeviceContainer dev_a_c = p2p.Install(a, c);
    NetDeviceContainer dev_b_d = p2p.Install(b, d);
    NetDeviceContainer dev_c_d = p2p.Install(c, d);
    NetDeviceContainer dev_d_dst = p2p.Install(d, dst);

    // Assign IP addresses to each subnet
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer iface_s_a, iface_a_b, iface_a_c, iface_b_d, iface_c_d, iface_d_dst;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    iface_s_a = ipv4.Assign(dev_s_a);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    iface_a_b = ipv4.Assign(dev_a_b);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    iface_a_c = ipv4.Assign(dev_a_c);

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    iface_b_d = ipv4.Assign(dev_b_d);

    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    iface_c_d = ipv4.Assign(dev_c_d);

    ipv4.SetBase("10.0.6.0", "255.255.255.0");
    iface_d_dst = ipv4.Assign(dev_d_dst);

    // Internet stack and RIP
    RipHelper rip;
    rip.ExcludeInterface(src, 1);   // Only static on SRC
    rip.ExcludeInterface(dst, 1);   // Only static on DST

    rip.Set("SplitHorizon", StringValue("PoisonReverse"));

    InternetStackHelper internet;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(rip, 0);
    listRH.Add(Ipv4StaticRoutingHelper(), 10);

    // Routers use RIP, ends use static
    internet.SetRoutingHelper(listRH);
    internet.Install(a);
    internet.Install(b);
    internet.Install(c);
    internet.Install(d);

    InternetStackHelper staticInternet;
    staticInternet.SetRoutingHelper(Ipv4StaticRoutingHelper());
    staticInternet.Install(src);
    staticInternet.Install(dst);

    // Set routing metric of 10 for c-d link
    Ptr<Ipv4> ipv4c = c->GetObject<Ipv4>();
    uint32_t ifIndex_c_d = ipv4c->GetInterfaceForDevice(dev_c_d.Get(0));
    ipv4c->SetMetric(ifIndex_c_d, 10);

    Ptr<Ipv4> ipv4d = d->GetObject<Ipv4>();
    uint32_t ifIndex_d_c = ipv4d->GetInterfaceForDevice(dev_c_d.Get(1));
    ipv4d->SetMetric(ifIndex_d_c, 10);

    // Static routes for SRC and DST
    Ptr<Ipv4StaticRouting> staticRoutingSrc = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(src->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRoutingSrc->AddNetworkRouteTo(Ipv4Address("10.0.6.0"), Ipv4Mask("255.255.255.0"), 1);

    Ptr<Ipv4StaticRouting> staticRoutingDst = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(dst->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRoutingDst->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), 1);

    // ICMP Ping from SRC to DST
    uint16_t port = 9;
    V4PingHelper pingHelper(iface_d_dst.GetAddress(1));
    pingHelper.SetAttribute("Verbose", BooleanValue(showPings));
    ApplicationContainer pingApps = pingHelper.Install(src);
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(90.0));

    // Simulate link failure: B-D at 40s; recover at 44s
    // Disable the B-D NetDevices from each endpoint
    Ptr<NetDevice> nd_b = dev_b_d.Get(0);
    Ptr<NetDevice> nd_d = dev_b_d.Get(1);

    Simulator::Schedule(Seconds(40.0), &NetDevice::SetLinkDown, nd_b);
    Simulator::Schedule(Seconds(40.0), &NetDevice::SetLinkDown, nd_d);
    Simulator::Schedule(Seconds(44.0), &NetDevice::SetLinkUp, nd_b);
    Simulator::Schedule(Seconds(44.0), &NetDevice::SetLinkUp, nd_d);

    // Print routing tables every 10 seconds if enabled
    if (printRoutingTables)
    {
        for (int t = 0; t <= 90; t += 10)
        {
            Simulator::Schedule(Seconds(t), &Ipv4RoutingHelper::PrintRoutingTableAllAt, Seconds(t), OutputStreamWrapper(&std::cout));
        }
    }

    Simulator::Stop(Seconds(92.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}