#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospfv2-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinkStateRoutingSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;
    bool animation = true;
    double simulationTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("animation", "Enable NetAnim XML trace file generation", animation);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (routers)
    NodeContainer routers;
    routers.Create(4);

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[4];

    // Connect router 0 to router 1
    routerDevices[0] = p2p.Install(routers.Get(0), routers.Get(1));
    // Connect router 1 to router 2
    routerDevices[1] = p2p.Install(routers.Get(1), routers.Get(2));
    // Connect router 2 to router 3
    routerDevices[2] = p2p.Install(routers.Get(2), routers.Get(3));
    // Connect router 3 to router 0 for a looped topology
    routerDevices[3] = p2p.Install(routers.Get(3), routers.Get(0));

    // Install Internet stack with OSPF routing
    Ospfv2Helper ospfRoutingHelper;
    ospfRoutingHelper.RedistributeConnected();
    Ipv4ListRoutingHelper listRH;
    listRH.Add(ospfRoutingHelper, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRH);
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfaces[4];
    for (int i = 0; i < 4; ++i) {
        routerInterfaces[i] = ipv4.Assign(routerDevices[i]);
        ipv4.NewNetwork();
    }

    // Add traffic: Echo server on router 3
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(routers.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Client on router 0 sending to router 3's IP
    UdpEchoClientHelper echoClient(routerInterfaces[3].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(routers.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable pcap tracing
    if (tracing) {
        p2p.EnablePcapAll("link-state-routing-simulation");
    }

    // Setup NetAnim
    if (animation) {
        AnimationInterface anim("link-state-routing.xml");
        anim.SetConstantPosition(routers.Get(0), 0.0, 0.0);
        anim.SetConstantPosition(routers.Get(1), 100.0, 0.0);
        anim.SetConstantPosition(routers.Get(2), 100.0, 100.0);
        anim.SetConstantPosition(routers.Get(3), 0.0, 100.0);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}