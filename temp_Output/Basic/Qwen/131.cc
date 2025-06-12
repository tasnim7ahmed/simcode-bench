#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinitySimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes for routers A, B, and C
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between A <-> B and B <-> C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack with Distance Vector routing
    InternetStackHelper stack;
    DvRoutingHelper dv;
    stack.SetRoutingHelper(dv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = ip.Assign(devAB);
    ip.NewNetwork();
    Ipv4InterfaceContainer ifBC = ip.Assign(devBC);

    // Set up static routes so all nodes can reach each other initially
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create a ping from node 0 (A) to node 2 (C)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(ifBC.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Schedule the link failure between B and C at time 5 seconds
    Simulator::Schedule(Seconds(5.0), &Ns3WifiPhyStateHelper::SetErrorRateModel,
                        devBC.Get(0)->GetPhy(), 0.0);
    Simulator::Schedule(Seconds(5.0), &Ns3WifiPhyStateHelper::SetErrorRateModel,
                        devBC.Get(1)->GetPhy(), 0.0);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}