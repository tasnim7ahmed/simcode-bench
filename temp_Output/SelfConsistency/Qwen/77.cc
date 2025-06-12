#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipNgRoutingSimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    bool printRoutingTables = false;
    bool splitHorizon = true; // Enable Split Horizon

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Print trace information if true", verbose);
    cmd.AddValue("printRoutingTables", "Print routing tables every 10 seconds if true", printRoutingTables);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6); // SRC, A, B, C, D, DST

    Ptr<Node> src = nodes.Get(0);
    Ptr<Node> a = nodes.Get(1);
    Ptr<Node> b = nodes.Get(2);
    Ptr<Node> c = nodes.Get(3);
    Ptr<Node> d = nodes.Get(4);
    Ptr<Node> dst = nodes.Get(5);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Define links and their costs
    NetDeviceContainer devSrcA = p2p.Install(src, a);
    NetDeviceContainer devAB = p2p.Install(a, b);
    NetDeviceContainer devAC = p2p.Install(a, c);
    NetDeviceContainer devBC = p2p.Install(b, c);
    NetDeviceContainer devBD = p2p.Install(b, d);
    NetDeviceContainer devCD = p2p.Install(c, d);
    NetDeviceContainer devDDst = p2p.Install(d, dst);

    InternetStackHelper internetv6;
    RipNgHelper ripNg;
    ripNg.Set("SplitHorizon", BooleanValue(splitHorizon)); // Enable Split Horizon
    internetv6.SetRoutingHelper(ripNg); // has effect only on nodes added after this call
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;

    // Assign IPv6 addresses to each link
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iSrcA = ipv6.Assign(devSrcA);
    iSrcA.SetForwarding(0, true);
    iSrcA.SetDefaultRouteInAllNodes(0);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iAB = ipv6.Assign(devAB);
    iAB.SetForwarding(0, true);
    iAB.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iAC = ipv6.Assign(devAC);
    iAC.SetForwarding(0, true);
    iAC.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iBC = ipv6.Assign(devBC);
    iBC.SetForwarding(0, true);
    iBC.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iBD = ipv6.Assign(devBD);
    iBD.SetForwarding(0, true);
    iBD.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iCD = ipv6.Assign(devCD);
    iCD.SetForwarding(0, true);
    iCD.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iDDst = ipv6.Assign(devDDst);
    iDDst.SetForwarding(0, true);
    iDDst.SetDefaultRouteInAllNodes(1);

    // Set link costs
    ripNg.ExcludeLink(src, a);
    ripNg.SetLinkMetric(devSrcA, 1);
    ripNg.SetLinkMetric(devAB, 1);
    ripNg.SetLinkMetric(devAC, 1);
    ripNg.SetLinkMetric(devBC, 1);
    ripNg.SetLinkMetric(devBD, 1); // Initially set to 1
    ripNg.SetLinkMetric(devCD, 10); // Cost of 10 for C-D link
    ripNg.SetLinkMetric(devDDst, 1);

    // Set up ping application from SRC to DST
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(dst);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient(iDDst.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(src);
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(60.0));

    // Schedule failure and recovery of B-D link
    Simulator::Schedule(Seconds(40.0), &Ipv6::SetDown, a->GetObject<Ipv6>(), devBD.Get(0));
    Simulator::Schedule(Seconds(44.0), &Ipv6::SetUp, a->GetObject<Ipv6>(), devBD.Get(0));
    Simulator::Schedule(Seconds(44.0), &Ipv6::SetDown, b->GetObject<Ipv6>(), devBD.Get(1));
    Simulator::Schedule(Seconds(44.0), &Ipv6::SetUp, b->GetObject<Ipv6>(), devBD.Get(1));

    // Tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng-routing-simulation.tr"));
    p2p.EnablePcapAll("ripng-routing-simulation");

    // Print routing tables periodically
    if (printRoutingTables) {
        ripNg.PrintRoutingTableAt(Seconds(10.0), nodes);
        ripNg.PrintRoutingTableAt(Seconds(20.0), nodes);
        ripNg.PrintRoutingTableAt(Seconds(30.0), nodes);
        ripNg.PrintRoutingTableAt(Seconds(40.0), nodes);
        ripNg.PrintRoutingTableAt(Seconds(50.0), nodes);
    }

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}