#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipSimpleRouting");

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool printRoutingTables = false;
    bool showPings = false;
    Time::SetResolution(Time::MS);

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable log components", verbose);
    cmd.AddValue("printRoutingTable", "Print routing tables at regular intervals", printRoutingTables);
    cmd.AddValue("showPings", "Show ping RTTs.", showPings);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RipSimpleRouting", LOG_LEVEL_INFO);
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4Interface", LOG_LEVEL_ALL);
    }

    // Create nodes
    Ptr<Node> src = CreateObject<Node>();
    Ptr<Node> dst = CreateObject<Node>();
    Ptr<Node> a = CreateObject<Node>();
    Ptr<Node> b = CreateObject<Node>();
    Ptr<Node> c = CreateObject<Node>();
    Ptr<Node> d = CreateObject<Node>();

    NodeContainer net(src, a, b, c, d, dst);
    NodeContainer routers(a, b, c, d);
    NodeContainer hosts(src, dst);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pHighCost;
    p2pHighCost.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHighCost.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devSrcA;
    NetDeviceContainer devAD;
    NetDeviceContainer devDB;
    NetDeviceContainer devBC;
    NetDeviceContainer devCD;
    NetDeviceContainer devDstC;

    devSrcA = p2p.Install(src, a);
    devAD = p2p.Install(a, d);
    devDB = p2p.Install(d, b);
    devBC = p2p.Install(b, c);
    devCD = p2pHighCost.Install(c, d);  // High cost link
    devDstC = p2p.Install(dst, c);

    InternetStackHelper stack;
    stack.SetRoutingProtocol("ns3::Rip");
    stack.Install(routers);
    stack.Install(hosts);

    Ipv4AddressHelper addr;
    addr.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifSrcA = addr.Assign(devSrcA);
    addr.NewNetwork();
    Ipv4InterfaceContainer ifAD = addr.Assign(devAD);
    addr.NewNetwork();
    Ipv4InterfaceContainer ifDB = addr.Assign(devDB);
    addr.NewNetwork();
    Ipv4InterfaceContainer ifBC = addr.Assign(devBC);
    addr.NewNetwork();
    Ipv4InterfaceContainer ifCD = addr.Assign(devCD);
    addr.NewNetwork();
    Ipv4InterfaceContainer ifDstC = addr.Assign(devDstC);

    // Enable split horizon with poison reverse
    Ptr<Rip> ripSrc = src->GetObject<Rip>();
    if (ripSrc) {
        ripSrc->SetAttribute("SplitHorizonStrategy", EnumValue(Rip::POISON_REVERSE));
    }

    for (NodeContainer::Iterator it = routers.Begin(); it != routers.End(); ++it) {
        Ptr<Rip> rip = (*it)->GetObject<Rip>();
        if (rip) {
            rip->SetAttribute("SplitHorizonStrategy", EnumValue(Rip::POISON_REVERSE));
        }
    }

    // Assign addresses
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Schedule routing table printing
    if (printRoutingTables) {
        Simulator::Schedule(Seconds(12), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(12));
        Simulator::Schedule(Seconds(24), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(24));
        Simulator::Schedule(Seconds(36), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(36));
        Simulator::Schedule(Seconds(48), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(48));
    }

    // Simulate link failure between B and D at 40 seconds
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkEnabled, devDB.Get(0), false);
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkEnabled, devDB.Get(1), false);

    // Recover the link after 44 seconds
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkEnabled, devDB.Get(0), true);
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkEnabled, devDB.Get(1), true);

    // Ping from SRC to DST
    V4PingHelper ping(dst->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
    ping.SetAttribute("Verbose", BooleanValue(showPings));
    ApplicationContainer apps = ping.Install(src);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(50.0));

    Simulator::Stop(Seconds(50));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}