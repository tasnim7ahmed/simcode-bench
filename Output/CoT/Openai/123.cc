#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-ospf-routing-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Square topology: 0-1-2-3-0
    // Links: 0-1, 1-2, 2-3, 3-0
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2_3 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d3_0 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack with OSPF routing
    InternetStackHelper internet;
    Ipv4OspfRoutingHelper ospfRouting;
    internet.SetRoutingHelper(ospfRouting);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0_1 = address.Assign(d0_1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1_2 = address.Assign(d1_2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2_3 = address.Assign(d2_3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i3_0 = address.Assign(d3_0);

    // UDP communication: node 0 sends to node 2
    uint16_t udpPort = 9;

    UdpEchoServerHelper echoServer(udpPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i2_3.GetAddress(1), udpPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("square-topology");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Print routing tables at end
    Ptr<Ipv4> ipv4;
    std::cout << "***** Routing tables at simulation end *****\n\n";
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        std::cout << "Node " << i << ":\n";
        ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ipv4RoutingTableEntry rt;
        Ptr<Ipv4RoutingProtocol> routing = ipv4->GetRoutingProtocol();
        routing->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout));
        std::cout << "\n";
    }

    Simulator::Destroy();
    return 0;
}