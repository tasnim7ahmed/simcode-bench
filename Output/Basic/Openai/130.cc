#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-nat-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NatBasicSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: h0, h1 (private hosts), nat, srv (public server)
    NodeContainer privateHosts;
    privateHosts.Create(2);

    Ptr<Node> nat = CreateObject<Node>();
    Ptr<Node> srv = CreateObject<Node>();

    // LAN: h0 <-> sw <-> h1
    // Build private network
    NodeContainer privateLan;
    privateLan.Add(privateHosts);
    privateLan.Add(nat);

    // Point-to-point helper for LAN and WAN
    PointToPointHelper p2pLan;
    p2pLan.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pLan.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper p2pWan;
    p2pWan.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pWan.SetChannelAttribute("Delay", StringValue("10ms"));

    // LAN devices: h0 <-> nat, h1 <-> nat
    NetDeviceContainer d_h0_nat = p2pLan.Install(NodeContainer(privateHosts.Get(0), nat));
    NetDeviceContainer d_h1_nat = p2pLan.Install(NodeContainer(privateHosts.Get(1), nat));

    // WAN link: nat <-> srv
    NetDeviceContainer d_nat_srv = p2pWan.Install(NodeContainer(nat, srv));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(privateHosts);
    stack.Install(nat);
    stack.Install(srv);

    // Assign IP addresses
    Ipv4AddressHelper ipPrivate1, ipPrivate2, ipPublic;
    ipPrivate1.SetBase("10.0.0.0", "255.255.255.0");
    ipPrivate2.SetBase("10.0.1.0", "255.255.255.0");
    ipPublic.SetBase("203.0.113.0", "255.255.255.0");

    // h0-nat: 10.0.0.1/24 (h0), 10.0.0.254/24 (nat)
    Ipv4InterfaceContainer if_h0_nat = ipPrivate1.Assign(d_h0_nat);
    // h1-nat: 10.0.1.1/24 (h1), 10.0.1.254/24 (nat)
    Ipv4InterfaceContainer if_h1_nat = ipPrivate2.Assign(d_h1_nat);
    // nat-srv: 203.0.113.1/24 (nat), 203.0.113.2/24 (srv)
    Ipv4InterfaceContainer if_nat_srv = ipPublic.Assign(d_nat_srv);

    // Set up NAT
    Ipv4NatHelper natHelper;
    Ptr<Ipv4Nat> natApp = natHelper.Install(nat);

    // Mapping of inside/outside interfaces
    // Private side: 10.0.0.254, 10.0.1.254   (interfaces 1,2)
    natApp->SetAttribute("InsideInterfaces", Ipv4InterfaceContainerValue(Ipv4InterfaceContainer({if_h0_nat.Get(1), if_h1_nat.Get(1)})));
    // Public side: 203.0.113.1   (interface 3)
    natApp->SetAttribute("OutsideInterfaces", Ipv4InterfaceContainerValue(Ipv4InterfaceContainer({if_nat_srv.Get(0)})));
    natApp->SetAttribute("AddressPool", Ipv4AddressValue("203.0.113.1"));

    // Set default routes on private hosts to NAT
    Ipv4StaticRoutingHelper sRouting;
    Ptr<Ipv4StaticRouting> host0Route = sRouting.GetStaticRouting(privateHosts.Get(0)->GetObject<Ipv4>());
    host0Route->SetDefaultRoute("10.0.0.254", 1);
    Ptr<Ipv4StaticRouting> host1Route = sRouting.GetStaticRouting(privateHosts.Get(1)->GetObject<Ipv4>());
    host1Route->SetDefaultRoute("10.0.1.254", 1);
    // Default route on NAT to public network
    Ptr<Ipv4StaticRouting> natRoute = sRouting.GetStaticRouting(nat->GetObject<Ipv4>());
    natRoute->AddNetworkRouteTo("203.0.113.0", "255.255.255.0", if_nat_srv.Get(1), 3);
    natRoute->AddNetworkRouteTo("10.0.0.0", "255.255.255.0", if_h0_nat.Get(0), 1);
    natRoute->AddNetworkRouteTo("10.0.1.0", "255.255.255.0", if_h1_nat.Get(0), 2);

    Ptr<Ipv4StaticRouting> srvRoute = sRouting.GetStaticRouting(srv->GetObject<Ipv4>());
    srvRoute->AddNetworkRouteTo("10.0.0.0", "255.255.255.0", if_nat_srv.Get(0), 1);
    srvRoute->AddNetworkRouteTo("10.0.1.0", "255.255.255.0", if_nat_srv.Get(0), 1);

    // Applications
    uint16_t echoPort = 9;
    // UDP Echo Server on srv
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(srv);
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(8.0));

    // UDP Echo Clients on h0, h1 => send to public IP (srv)
    UdpEchoClientHelper echoClient0(if_nat_srv.GetAddress(1), echoPort);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(64));

    UdpEchoClientHelper echoClient1(if_nat_srv.GetAddress(1), echoPort);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps0 = echoClient0.Install(privateHosts.Get(0));
    ApplicationContainer clientApps1 = echoClient1.Install(privateHosts.Get(1));
    clientApps0.Start(Seconds(1.0));
    clientApps0.Stop(Seconds(7.0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(7.0));

    // Enable pcap
    p2pLan.EnablePcapAll("nat-private");
    p2pWan.EnablePcapAll("nat-public");

    // Log echo replies for verification
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Received", MakeCallback([](std::string context, Ptr<const Packet> packet) {
        NS_LOG_UNCOND("Client received echo reply: " << packet->GetSize() << " bytes");
    }));

    Simulator::Stop(Seconds(8.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}