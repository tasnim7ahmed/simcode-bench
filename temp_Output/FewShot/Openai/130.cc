#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-nat-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4Nat", LOG_LEVEL_INFO);

    // Create nodes: 2 private hosts, 1 NAT router, 1 public server
    NodeContainer privateHosts;
    privateHosts.Create(2);
    Ptr<Node> natRouter = CreateObject<Node>();
    Ptr<Node> publicServer = CreateObject<Node>();

    // Point-to-point between private hosts and NAT router
    NodeContainer privateNet;
    privateNet.Add(privateHosts);
    privateNet.Add(natRouter);

    PointToPointHelper p2pPrivate;
    p2pPrivate.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pPrivate.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer privateDevices1 = p2pPrivate.Install(privateHosts.Get(0), natRouter);
    NetDeviceContainer privateDevices2 = p2pPrivate.Install(privateHosts.Get(1), natRouter);

    // Point-to-point between NAT router and public server
    PointToPointHelper p2pPublic;
    p2pPublic.SetDeviceAttribute("DataRate", StringValue("20Mbps"));
    p2pPublic.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer publicDevices = p2pPublic.Install(natRouter, publicServer);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(privateHosts);
    stack.Install(natRouter);
    stack.Install(publicServer);

    // Assign private addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifPrivate1 = address.Assign(privateDevices1);

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifPrivate2 = address.Assign(privateDevices2);

    // Assign public addresses
    address.SetBase("203.0.113.0", "255.255.255.0");
    Ipv4InterfaceContainer ifPublic = address.Assign(publicDevices);

    // Set up NAT on NAT router (private is interfaces 1 and 2, public is interface 0)
    // Get device mapping: assume devices installed in order; check with actual Install() calls if unsure
    Ptr<Ipv4> ipv4Nat = natRouter->GetObject<Ipv4>();
    uint32_t publicIfIndex = ipv4Nat->GetInterfaceForDevice(publicDevices.Get(0));
    uint32_t privateIfIndex1 = ipv4Nat->GetInterfaceForDevice(privateDevices1.Get(1));
    uint32_t privateIfIndex2 = ipv4Nat->GetInterfaceForDevice(privateDevices2.Get(1));

    Ipv4NatHelper natHelper;
    Ptr<Ipv4Nat> nat = natHelper.Install(natRouter, publicIfIndex);

    // Add private interfaces to NAT
    nat->SetAttribute("IdleTimeout", TimeValue(Seconds(30)));
    nat->AddPrivateInterface(privateIfIndex1);
    nat->AddPrivateInterface(privateIfIndex2);

    // Set default routes for hosts: via NAT router's private IP
    Ipv4StaticRoutingHelper staticRouting;
    Ptr<Ipv4StaticRouting> routing1 = staticRouting.GetStaticRouting(privateHosts.Get(0)->GetObject<Ipv4>());
    routing1->SetDefaultRoute(ifPrivate1.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> routing2 = staticRouting.GetStaticRouting(privateHosts.Get(1)->GetObject<Ipv4>());
    routing2->SetDefaultRoute(ifPrivate2.GetAddress(1), 1);

    // Set default route for NAT router (to public server)
    Ptr<Ipv4StaticRouting> natRouting = staticRouting.GetStaticRouting(ipv4Nat);
    natRouting->SetDefaultRoute(ifPublic.GetAddress(1), publicIfIndex);

    // For the public server, add a route back to the private net via the NAT public IP (typically public network is default)
    Ptr<Ipv4StaticRouting> srvRouting = staticRouting.GetStaticRouting(publicServer->GetObject<Ipv4>());
    srvRouting->SetDefaultRoute("203.0.113.1", 1); // NAT router's public address

    // UDP Echo server on public server
    uint16_t echoPort = 5000;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(publicServer);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP Echo clients on private hosts to public server
    UdpEchoClientHelper echoClient(ifPublic.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(256));

    ApplicationContainer clientApps1 = echoClient.Install(privateHosts.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(18.0));

    ApplicationContainer clientApps2 = echoClient.Install(privateHosts.Get(1));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(18.0));

    // Enable pcap tracing for verification
    p2pPrivate.EnablePcapAll("nat-private");
    p2pPublic.EnablePcapAll("nat-public");

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}