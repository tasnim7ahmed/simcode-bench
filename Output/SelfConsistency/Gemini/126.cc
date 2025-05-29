#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dce-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistanceVectorRoutingSimulation");

int main(int argc, char* argv[]) {
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_ALL);
    LogComponent::SetDefaultLogComponentEnable(true);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer routers;
    routers.Create(3);

    NodeContainer subnets[4];
    for (int i = 0; i < 4; ++i) {
        subnets[i].Create(1); // One node in each subnet
    }

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer r0r1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer r0r2 = p2p.Install(routers.Get(0), routers.Get(2));
    NetDeviceContainer r1r2 = p2p.Install(routers.Get(1), routers.Get(2));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(routers);
    for (int i = 0; i < 4; ++i) {
        internet.Install(subnets[i]);
    }

    // Assign IPv4 addresses
    Ipv4AddressHelper address;

    // Router addresses
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r0r1 = address.Assign(r0r1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r0r2 = address.Assign(r0r2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1r2 = address.Assign(r1r2);

    // Subnet addresses
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i_sub0 = address.Assign(p2p.Install(subnets[0].Get(0), routers.Get(0)));

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i_sub1 = address.Assign(p2p.Install(subnets[1].Get(0), routers.Get(1)));

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i_sub2 = address.Assign(p2p.Install(subnets[2].Get(0), routers.Get(2)));

    address.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer i_sub3 = address.Assign(p2p.Install(subnets[3].Get(0), routers.Get(0)));

    // Enable global routing (necessary for traffic between different subnets)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create Applications (Traffic)
    uint16_t port = 9; // Discard port (RFC 863)

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(subnets[2].Get(0)); // Server in subnet 2
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i_sub2.GetAddress(1), port); // Client in subnet 0, sending to server in subnet 2
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(subnets[0].Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));


    // Add pcap tracing
    p2p.EnablePcapAll("dvr_simulation");

    //AnimationInterface anim("dvr_animation.xml");
    // Run the simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}