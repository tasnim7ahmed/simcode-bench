#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("OspfRouter", LOG_LEVEL_INFO);

    // Create 4 router nodes
    NodeContainer routers;
    routers.Create(4);

    // Install the internet stack with OSPF support
    // OSPFHelper is assumed available in NS-3 ≥ 3.41
    OspfHelper ospf;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospf, 0);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(routers);

    // Set up a square topology:
    //
    //   n0---n1
    //   |     |
    //   n3---n2

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Create links
    NetDeviceContainer d01 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer d12 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer d23 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer d30 = p2p.Install(routers.Get(3), routers.Get(0));
    NetDeviceContainer d02 = p2p.Install(routers.Get(0), routers.Get(2)); // Optional diagonal for more dynamics

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = address.Assign(d01);
    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = address.Assign(d12);
    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = address.Assign(d23);
    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i30 = address.Assign(d30);
    address.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i02 = address.Assign(d02);

    // Enable pcap tracing to visualize LSA and other traffic
    p2p.EnablePcapAll("ospf-lsa");

    // Create UDP traffic generator: node 0 sends packets to node 2
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(routers.Get(2));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(25.0));

    UdpClientHelper udpClient(i12.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = udpClient.Install(routers.Get(0));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(25.0));

    // Simulate a link failure event to trigger LSA updates
    Simulator::Schedule(Seconds(10.0), [&]() {
        // Bring down link between node 1 and node 2
        Ptr<NetDevice> nd1 = d12.Get(0);
        Ptr<NetDevice> nd2 = d12.Get(1);
        nd1->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        nd2->SetReceiveCallback(MakeNullCallback<bool, Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &>());
        nd1->SetLinkDown();
        nd2->SetLinkDown();
        std::cout << "Link between n1 and n2 is down at " << Simulator::Now().GetSeconds() << "s" << std::endl;
    });

    Simulator::Schedule(Seconds(18.0), [&]() {
        // Bring link between node 1 and node 2 back up
        Ptr<NetDevice> nd1 = d12.Get(0);
        Ptr<NetDevice> nd2 = d12.Get(1);
        nd1->SetLinkUp();
        nd2->SetLinkUp();
        std::cout << "Link between n1 and n2 is up at " << Simulator::Now().GetSeconds() << "s" << std::endl;
    });

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}