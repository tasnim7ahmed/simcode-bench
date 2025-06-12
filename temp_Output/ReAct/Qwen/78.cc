#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipSimpleRouting");

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool printRoutingTables = false;
    bool showPings = false;
    Time::SetResolution(Time::MS);

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.AddValue("printRoutingTable", "Print routing tables at 30, 60 seconds", printRoutingTables);
    cmd.AddValue("showPings", "Show ping RTTs", showPings);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("RipSimpleRouting", LOG_LEVEL_INFO);
        LogComponentEnable("Rip", LOG_LEVEL_ALL);
    }

    // Create nodes
    Ptr<Node> src = CreateObject<Node>();
    Ptr<Node> dst = CreateObject<Node>();
    Ptr<Node> a = CreateObject<Node>();
    Ptr<Node> b = CreateObject<Node>();
    Ptr<Node> c = CreateObject<Node>();
    Ptr<Node> d = CreateObject<Node>();

    NodeContainer nodeSet(src, a, b, c, d, dst);

    // Link configuration
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    // Links: SRC -> A
    NetDeviceContainer devSrcA = p2p.Install(src, a);
    // Links: A -> B
    NetDeviceContainer devAB = p2p.Install(a, b);
    // Links: B -> D
    NetDeviceContainer devBD = p2p.Install(b, d);
    // Links: D -> C
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(100)));
    NetDeviceContainer devDC = p2p.Install(d, c);
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    // Links: C -> A
    NetDeviceContainer devCA = p2p.Install(c, a);
    // Links: D -> DST
    NetDeviceContainer devDstD = p2p.Install(dst, d);

    InternetStackHelper internet;
    RipHelper rip;
    rip.SetInterfaceMetric(devDC.Get(0), 10); // CD link cost 10
    rip.SetSplitHorizonStrategy(RipNg::SPLIT_HORIZON_POISON_REVERSE);

    internet.SetRoutingHelper(rip);
    internet.Install(nodeSet);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifSrcA = ipv4.Assign(devSrcA);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifBD = ipv4.Assign(devBD);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifDC = ipv4.Assign(devDC);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifCA = ipv4.Assign(devCA);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer ifDstD = ipv4.Assign(devDstD);

    // Server on DST
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(dst);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(70.0));

    // Client on SRC
    UdpEchoClientHelper echoClient(ifDstD.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(src);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(70.0));

    if (showPings) {
        V4PingHelper pingHelper(ifDstD.GetAddress(0));
        pingHelper.SetAttribute("Verbose", BooleanValue(true));
        ApplicationContainer pingApp = pingHelper.Install(src);
        pingApp.Start(Seconds(2.0));
        pingApp.Stop(Seconds(70.0));
    }

    if (printRoutingTables) {
        rip.PrintRoutingTableAt(Seconds(30.0), a);
        rip.PrintRoutingTableAt(Seconds(30.0), b);
        rip.PrintRoutingTableAt(Seconds(30.0), c);
        rip.PrintRoutingTableAt(Seconds(30.0), d);
        rip.PrintRoutingTableAt(Seconds(60.0), a);
        rip.PrintRoutingTableAt(Seconds(60.0), b);
        rip.PrintRoutingTableAt(Seconds(60.0), c);
        rip.PrintRoutingTableAt(Seconds(60.0), d);
    }

    // Simulate failure between B and D at 40s
    Simulator::Schedule(Seconds(40.0), &Ipv4::SetDown, b->GetObject<Ipv4>(), devBD.Get(1)->GetIfIndex());
    Simulator::Schedule(Seconds(40.0), &Ipv4::SetDown, d->GetObject<Ipv4>(), devBD.Get(0)->GetIfIndex());

    // Recover link between B and D at 44s
    Simulator::Schedule(Seconds(44.0), &Ipv4::SetUp, b->GetObject<Ipv4>(), devBD.Get(1)->GetIfIndex());
    Simulator::Schedule(Seconds(44.0), &Ipv4::SetUp, d->GetObject<Ipv4>(), devBD.Get(0)->GetIfIndex());

    Simulator::Stop(Seconds(70.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}