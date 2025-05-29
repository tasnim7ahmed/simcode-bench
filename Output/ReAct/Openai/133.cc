#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: Node0 (A), Node1 (B), Node2 (C)
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link A-B
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Link B-C
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Install Internet stack with DV routing, enabling Split Horizon
    InternetStackHelper internet;
    DvRoutingHelper dv;
    dv.Set ("SplitHorizon", BooleanValue(true)); // Enable Split Horizon

    internet.SetRoutingHelper(dv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    // Enable routing table printing
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    dv.PrintRoutingTableAt(Seconds(1.0), nodes.Get(0), routingStream);
    dv.PrintRoutingTableAt(Seconds(1.0), nodes.Get(1), routingStream);
    dv.PrintRoutingTableAt(Seconds(1.0), nodes.Get(2), routingStream);

    dv.PrintRoutingTableAt(Seconds(2.5), nodes.Get(0), routingStream);
    dv.PrintRoutingTableAt(Seconds(2.5), nodes.Get(1), routingStream);
    dv.PrintRoutingTableAt(Seconds(2.5), nodes.Get(2), routingStream);

    // Install UDP echo server on Node 2 (C)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(8.0));

    // Install UDP echo client on Node 0 (A)
    UdpEchoClientHelper echoClient(i1i2.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(8.0));

    // Tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("split-horizon-dv.tr"));

    Simulator::Stop(Seconds(9.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}