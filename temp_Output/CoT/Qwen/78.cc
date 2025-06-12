#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/rip-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipSimpleRouting");

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool printRoutingTables = false;
    bool showPings = false;
    Time::SetResolution(Time::US);

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Print trace information if true", verbose);
    cmd.AddValue("printRoutingTable", "Print routing tables at regular intervals", printRoutingTables);
    cmd.AddValue("showPings", "Show ping RTTs", showPings);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes
    Ptr<Node> src = CreateObject<Node>();
    Ptr<Node> dst = CreateObject<Node>();
    Ptr<Node> a = CreateObject<Node>();
    Ptr<Node> b = CreateObject<Node>();
    Ptr<Node> c = CreateObject<Node>();
    Ptr<Node> d = CreateObject<Node>();

    NodeContainer nodeContainer;
    nodeContainer.Add(src);
    nodeContainer.Add(dst);
    nodeContainer.Add(a);
    nodeContainer.Add(b);
    nodeContainer.Add(c);
    nodeContainer.Add(d);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devSrcA = p2p.Install(src, a);
    NetDeviceContainer devAD = p2p.Install(a, d);
    NetDeviceContainer devAB = p2p.Install(a, b);
    NetDeviceContainer devBD = p2p.Install(b, d);
    NetDeviceContainer devBC = p2p.Install(b, c);
    NetDeviceContainer devCD = p2p.Install(c, d);

    // Special link between C and D with cost 10
    PointToPointHelper p2pCost10;
    p2pCost10.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pCost10.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devCDHighCost = p2pCost10.Install(c, d);

    // Install Internet stack with RIP routing
    RipHelper ripRouting;
    ripRouting.SetInterfaceMetric(devCD.Get<NetDevice>(0), 10);
    ripRouting.SetInterfaceMetric(devCDHighCost.Get<NetDevice>(0), 10);

    ripRouting.ExcludeInterface(a, 1); // Exclude interface connected to src
    ripRouting.ExcludeInterface(dst, 1); // Exclude interface connected to dst

    InternetStackHelper stack;
    stack.SetRoutingHelper(ripRouting);
    stack.Install(nodeContainer);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifSrcA = ipv4.Assign(devSrcA);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAD = ipv4.Assign(devAD);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = ipv4.Assign(devAB);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBD = ipv4.Assign(devBD);

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBC = ipv4.Assign(devBC);

    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifCD = ipv4.Assign(devCD);

    ipv4.SetBase("10.0.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ifCDHighCost = ipv4.Assign(devCDHighCost);

    // Enable split horizon with poison reverse
    for (auto node : nodeContainer) {
        if (node == src || node == dst) continue;
        Ptr<Ipv4> ipv4Node = node->GetObject<Ipv4>();
        Ptr<Ipv4RoutingProtocol> rp = ipv4Node->GetRoutingProtocol();
        Ptr<Rip> rip_rp = rp->GetObject<Rip>();
        if (rip_rp) {
            rip_rp->SetSplitHorizonStrategy(Rip::POISON_REVERSE);
        }
    }

    // Ping application from src to dst
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.);
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(dst);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient(ifCDHighCost.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(src);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(60.0));

    if (showPings) {
        AsciiTraceHelper ascii;
        p2p.EnableAsciiAll(ascii.CreateFileStream("rip-simple-routing-ping.tr"));
    }

    // Schedule link failure between B and D
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkEnabled, devBD.Get(0), false);
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkEnabled, devBD.Get(1), false);

    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkEnabled, devBD.Get(0), true);
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkEnabled, devBD.Get(1), true);

    // Print routing tables periodically
    if (printRoutingTables) {
        Ipv4GlobalRoutingHelper::PrintRoutingTableEvery(Seconds(4), a, CreateFileStream("a-routing-table.txt"));
        Ipv4GlobalRoutingHelper::PrintRoutingTableEvery(Seconds(4), b, CreateFileStream("b-routing-table.txt"));
        Ipv4GlobalRoutingHelper::PrintRoutingTableEvery(Seconds(4), c, CreateFileStream("c-routing-table.txt"));
        Ipv4GlobalRoutingHelper::PrintRoutingTableEvery(Seconds(4), d, CreateFileStream("d-routing-table.txt"));
    }

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}