#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dvdv-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for DV routing
    LogComponentEnable("DvdvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer routers;
    routers.Create(3);

    NodeContainer subnets[4];
    for (int i = 0; i < 4; ++i) {
        subnets[i].Create(1);
    }

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerDevices[3];
    routerDevices[0] = p2p.Install(routers.Get(0), routers.Get(1));
    routerDevices[1] = p2p.Install(routers.Get(0), routers.Get(2));
    routerDevices[2] = p2p.Install(routers.Get(1), routers.Get(2));

    NetDeviceContainer subnetDevices[4];
    subnetDevices[0] = p2p.Install(routers.Get(0), subnets[0].Get(0));
    subnetDevices[1] = p2p.Install(routers.Get(0), subnets[1].Get(0));
    subnetDevices[2] = p2p.Install(routers.Get(1), subnets[2].Get(0));
    subnetDevices[3] = p2p.Install(routers.Get(2), subnets[3].Get(0));


    // Install Internet stack
    InternetStackHelper internet;
    internet.SetRoutingHelper(DvdvHelper());
    internet.Install(routers);

    for (int i = 0; i < 4; ++i) {
        internet.Install(subnets[i]);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer routerInterfaces[3];
    address.SetBase("10.1.1.0", "255.255.255.0");
    routerInterfaces[0] = address.Assign(routerDevices[0]);
    address.SetBase("10.1.2.0", "255.255.255.0");
    routerInterfaces[1] = address.Assign(routerDevices[1]);
    address.SetBase("10.1.3.0", "255.255.255.0");
    routerInterfaces[2] = address.Assign(routerDevices[2]);

    Ipv4InterfaceContainer subnetInterfaces[4];
    address.SetBase("10.1.4.0", "255.255.255.0");
    subnetInterfaces[0] = address.Assign(subnetDevices[0]);
    address.SetBase("10.1.5.0", "255.255.255.0");
    subnetInterfaces[1] = address.Assign(subnetDevices[1]);
    address.SetBase("10.1.6.0", "255.255.255.0");
    subnetInterfaces[2] = address.Assign(subnetDevices[2]);
    address.SetBase("10.1.7.0", "255.255.255.0");
    subnetInterfaces[3] = address.Assign(subnetDevices[3]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create Applications (Traffic)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(subnets[3].Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(subnetInterfaces[3].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(subnets[0].Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("dvdv");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}