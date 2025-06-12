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
    cmd.AddValue("verbose", "Print trace information if true", verbose);
    cmd.AddValue("printRoutingTable", "Print routing tables at regular intervals", printRoutingTables);
    cmd.AddValue("showPings", "Show ping traces", showPings);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RipSimpleRouting", LOG_LEVEL_INFO);
        LogComponentEnable("RipHelper", LOG_LEVEL_ALL);
        LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_WARN);
    }

    // Create nodes
    NodeContainer srcNode;
    srcNode.Create(1);
    NodeContainer dstNode;
    dstNode.Create(1);
    NodeContainer routers;
    routers.Create(4); // A, B, C, D

    // Define links
    NodeContainer linkSrcA = NodeContainer(srcNode.Get(0), routers.Get(0)); // SRC <-> A
    NodeContainer linkAB = NodeContainer(routers.Get(0), routers.Get(1));   // A <-> B
    NodeContainer linkBD = NodeContainer(routers.Get(1), routers.Get(3));   // B <-> D
    NodeContainer linkAD = NodeContainer(routers.Get(0), routers.Get(3));   // A <-> D
    NodeContainer linkCD = NodeContainer(routers.Get(2), routers.Get(3));   // C <-> D
    NodeContainer linkDstC = NodeContainer(dstNode.Get(0), routers.Get(2)); // DST <-> C

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesSrcA = p2p.Install(linkSrcA);
    NetDeviceContainer devicesAB = p2p.Install(linkAB);
    NetDeviceContainer devicesBD = p2p.Install(linkBD);
    NetDeviceContainer devicesAD = p2p.Install(linkAD);
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));  // Higher delay for CD to simulate higher cost
    NetDeviceContainer devicesCD = p2p.Install(linkCD);
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));    // Reset to default
    NetDeviceContainer devicesDstC = p2p.Install(linkDstC);

    InternetStackHelper stack;
    RipHelper rip;
    rip.SetInterfaceMetric(linkCD.Get(0), 10); // Set metric of CD link to 10
    rip.SetSplitHorizonStrategy(RipNg::POISON_REVERSE); // Poison reverse
    stack.SetRoutingHelper(rip);
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesSrcA = address.Assign(devicesSrcA);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesBD = address.Assign(devicesBD);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesAD = address.Assign(devicesAD);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesCD = address.Assign(devicesCD);
    address.NewNetwork();
    Ipv4InterfaceContainer interfacesDstC = address.Assign(devicesDstC);

    // Enable routing tables
    if (printRoutingTables) {
        rip.PrintRoutingTableAt(Seconds(30), NodeList::GetNode(0));
        rip.PrintRoutingTableEvery(Seconds(10), NodeList::GetNode(0), Seconds(60));
    }

    // Install ping application from SRC to DST
    V4PingHelper ping(dstNode.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ping.SetAttribute("Verbose", BooleanValue(showPings));
    ApplicationContainer srcApp = ping.Install(srcNode);
    srcApp.Start(Seconds(1.0));
    srcApp.Stop(Seconds(70.0));

    // Link failure between B and D at 40s, recovery at 44s
    Simulator::Schedule(Seconds(40.0), &Ipv4InterfaceContainer::DoUpOrDown, &interfacesBD, false);
    Simulator::Schedule(Seconds(44.0), &Ipv4InterfaceContainer::DoUpOrDown, &interfacesBD, true);

    Simulator::Stop(Seconds(70.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}