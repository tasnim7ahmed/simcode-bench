#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TreeTopologySimulation", LOG_LEVEL_INFO);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer root;
    root.Create(1);

    NodeContainer firstLevelChildren;
    firstLevelChildren.Create(2);

    NodeContainer leaf;
    leaf.Create(1);

    NS_LOG_INFO("Creating point-to-point links.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer rootToChild1 = p2p.Install(root.Get(0), firstLevelChildren.Get(0));
    NetDeviceContainer rootToChild2 = p2p.Install(root.Get(0), firstLevelChildren.Get(1));
    NetDeviceContainer child1ToLeaf = p2p.Install(firstLevelChildren.Get(0), leaf.Get(0));

    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper internet;
    internet.InstallAll();

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer rootInterfaces1 = ipv4.Assign(rootToChild1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer rootInterfaces2 = ipv4.Assign(rootToChild2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer child1LeafInterfaces = ipv4.Assign(child1ToLeaf);

    NS_LOG_INFO("Enabling global routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Setting up applications.");
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(root.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(rootInterfaces1.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(leaf.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation completed.");

    return 0;
}