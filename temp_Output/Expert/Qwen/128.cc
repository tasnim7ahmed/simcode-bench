#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospfv2-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinkStateRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (routers)
    NodeContainer routers;
    routers.Create(4);

    // Point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[6];
    routerDevices[0] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    routerDevices[1] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(2)));
    routerDevices[2] = p2p.Install(NodeContainer(routers.Get(1), routers.Get(3)));
    routerDevices[3] = p2p.Install(NodeContainer(routers.Get(2), routers.Get(3)));
    routerDevices[4] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(3)));
    routerDevices[5] = p2p.Install(NodeContainer(routers.Get(1), routers.Get(2)));

    // Install Internet stack with OSPF routing
    InternetStackHelper internet;
    Ospfv2Helper ospfRoutingHelper;

    // Configure OSPF areas and interfaces
    ipv4RoutingHelper::OspfArea area0 = ospfRoutingHelper.CreateArea(0);
    ospfRoutingHelper.ExcludeInterface(routers.Get(0), 0); // Exclude loopback

    internet.SetRoutingInstall(true);
    internet.SetRoutingProtocol(&ospfRoutingHelper, Ipv4Address::GetZero(), Ipv4Mask::GetZero());
    internet.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");
    for (int i = 0; i < 6; ++i) {
        ip.Assign(routerDevices[i]);
        ip.NewNetwork();
    }

    // Create a host node connected to Router 0
    NodeContainer host;
    host.Create(1);
    NetDeviceContainer hostDevice = p2p.Install(routers.Get(0), host.Get(0));
    ip.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer hostIp = ip.Assign(hostDevice);
    internet.Install(host);

    // Another host on Router 3
    NodeContainer host2;
    host2.Create(1);
    NetDeviceContainer hostDevice2 = p2p.Install(routers.Get(3), host2.Get(0));
    ip.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer hostIp2 = ip.Assign(hostDevice2);
    internet.Install(host2);

    // Set up echo server on host2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(host2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up echo client from host to host2
    UdpEchoClientHelper echoClient(hostIp2.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(host.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("lsa_simulation");

    // Schedule simulation events
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}