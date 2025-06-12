#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    uint32_t nNodes = 4;

    NodeContainer nodes;
    nodes.Create(nNodes);

    // Set up node locations for NetAnim visualization
    std::vector<Vector> nodePositions = {Vector(50, 50, 0), Vector(250, 50, 0), Vector(250, 250, 0), Vector(50, 250, 0)};

    // Create point-to-point links: 0-1, 1-2, 2-3, 3-0, 0-2
    std::vector<std::pair<uint32_t, uint32_t>> links = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {0, 2}
    };

    // Point-to-point channel parameters
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    for (auto const &link : links)
    {
        NodeContainer nc = NodeContainer(nodes.Get(link.first), nodes.Get(link.second));
        deviceContainers.push_back(p2p.Install(nc));
    }

    // Install internet stack with OSPF routing on all nodes
    OspfHelper ospfRouting;
    ospfRouting.Set("RouterPriority", UintegerValue(1));
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospfRouting);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;
    for (uint32_t i = 0; i < deviceContainers.size(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        interfaces.push_back(address.Assign(deviceContainers[i]));
    }

    // Set up UDP Echo Server on node 2
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0 targeting node 2
    UdpEchoClientHelper echoClient(interfaces[1].GetAddress(1), echoPort); // 0-1-2: use link 1 to reach node2
    echoClient.SetAttribute("MaxPackets", UintegerValue(6));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // NetAnim config
    AnimationInterface anim("ospf-netanim.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        anim.SetConstantPosition(nodes.Get(i), nodePositions[i].x, nodePositions[i].y);
    }
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("ospf-routes.xml", Seconds(0), Seconds(10), Seconds(0.5));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}