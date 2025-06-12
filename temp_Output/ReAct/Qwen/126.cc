#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DVRSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 routers and 4 end hosts (one in each subnetwork)
    NodeContainer routers;
    routers.Create(3);

    NodeContainer hosts;
    hosts.Create(4);

    // Point-to-point links between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[3][3];

    // Connect routers in a triangle topology
    routerDevices[0][1] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(1)));
    routerDevices[1][2] = p2p.Install(NodeContainer(routers.Get(1), routers.Get(2)));
    routerDevices[0][2] = p2p.Install(NodeContainer(routers.Get(0), routers.Get(2)));

    // Connect hosts to routers (each host connected to one router)
    NetDeviceContainer hostDevices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        hostDevices[i] = p2p.Install(NodeContainer(hosts.Get(i), routers.Get(i % 3)));
    }

    InternetStackHelper internet;
    DvRoutingHelper dv;
    internet.SetRoutingHelper(dv); // has effect on nodes added after this call

    internet.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer routerInterfaces[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            routerInterfaces[i][j] = ip.Assign(routerDevices[i][j]);
            ip.NewNetwork();
        }
    }

    Ipv4InterfaceContainer hostInterfaces[4];
    for (uint32_t i = 0; i < 4; ++i) {
        hostInterfaces[i] = ip.Assign(hostDevices[i]);
        ip.NewNetwork();
    }

    // Create UDP echo server on host 0
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create UDP echo client on host 2 sending to host 0
    UdpEchoClientHelper echoClient(hostInterfaces[0].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(hosts.Get(2));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing for all devices
    p2p.EnablePcapAll("dvr_simulation");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}