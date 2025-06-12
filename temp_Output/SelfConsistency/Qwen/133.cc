#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SplitHorizonTest");

int main(int argc, char *argv[]) {
    bool verbose = true;
    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Print trace information if true", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Assign IP addresses
    InternetStackHelper internet;
    DvRoutingHelper dv;
    dv.Set("SplitHorizon", BooleanValue(true));  // Enable Split Horizon

    Ipv4ListRoutingHelper list;
    list.Add(dv, 0);  // Add Distance Vector routing at priority 0

    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    ip.Assign(devAB);

    ip.SetBase("10.1.2.0", "255.255.255.0");
    ip.Assign(devBC);

    // Ensure all routing tables are built
    Simulator::Schedule(Seconds(1.0), &dv::DvRoutingProtocol::PrintRoutingTableAllNodes, Nodes());

    // Set up a simple echo server and client to test connectivity
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ip.GetAddress(1, 0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}