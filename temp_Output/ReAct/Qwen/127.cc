#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 routers
    NodeContainer routers;
    routers.Create(4);

    // Define point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect the routers in a square topology: 0-1, 1-2, 2-3, 3-0
    NetDeviceContainer link01 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer link12 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer link23 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer link30 = p2p.Install(routers.Get(3), routers.Get(0));

    InternetStackHelper stack;
    Ipv4ListRoutingHelper listRH;
    OspfHelper ospf;

    // Set OSPF area 0 for all routers
    listRH.Add(ospf, 0);
    stack.SetRoutingHelper(listRH);
    stack.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(link01);

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(link12);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(link23);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i0 = ipv4.Assign(link30);

    // Create a UDP echo server on node 2 (destination)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(routers.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create a UDP echo client on node 0 (source)
    UdpEchoClientHelper echoClient(i2i3.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(routers.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    if (tracing) {
        p2p.EnablePcapAll("ospf-routing");
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}