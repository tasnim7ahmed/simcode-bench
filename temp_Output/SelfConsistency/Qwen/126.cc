#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h" // Distance Vector Routing (DSDV) for dynamic routing

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DistanceVectorRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 routers and 4 end hosts (1 per subnet)
    NodeContainer routers;
    routers.Create(3);

    NodeContainer hosts;
    hosts.Create(4);

    // Create point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[3];

    // Connect routers in a chain: R0 <-> R1 <-> R2
    routerDevices[0] = p2p.Install(routers.Get(0), routers.Get(1));
    routerDevices[1] = p2p.Install(routers.Get(1), routers.Get(2));

    // Connect each host to one of the routers
    routerDevices[2] = p2p.Install(hosts.Get(0), routers.Get(0)); // Host 0 -> R0
    routerDevices[3] = p2p.Install(hosts.Get(1), routers.Get(0)); // Host 1 -> R0
    routerDevices[4] = p2p.Install(hosts.Get(2), routers.Get(2)); // Host 2 -> R2
    routerDevices[5] = p2p.Install(hosts.Get(3), routers.Get(2)); // Host 3 -> R2

    InternetStackHelper internet;
    internet.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ipv4.Assign(routerDevices[0]); // R0 <-> R1 link

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ipv4.Assign(routerDevices[1]); // R1 <-> R2 link

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    ipv4.Assign(routerDevices[2]); // Host0 <-> R0

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ipv4.Assign(routerDevices[3]); // Host1 <-> R0

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    ipv4.Assign(routerDevices[4]); // Host2 <-> R2

    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    ipv4.Assign(routerDevices[5]); // Host3 <-> R2

    // Enable DSDV (Distance Sequence Distance Vector) routing protocol
    DsdvHelper dsdv;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(dsdv, 10); // priority 10

    InternetStackHelper stack;
    stack.SetRoutingHelper(listRH);
    stack.Install(routers);

    // Set up echo server on Host 3 (IP will be assigned dynamically)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up echo client on Host 0
    UdpEchoClientHelper echoClient(hosts.Get(3)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(hosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("dvr-routing");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}