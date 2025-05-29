#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes for different topologies
    // 1. Ring: 4 nodes (0-3)
    NodeContainer ringNodes;
    ringNodes.Create(4);

    // 2. Mesh: 3 nodes (4-6)
    NodeContainer meshNodes;
    meshNodes.Create(3);

    // 3. Bus: 4 nodes (7-10)
    NodeContainer busNodes;
    busNodes.Create(4);

    // 4. Line: 3 nodes (11-13)
    NodeContainer lineNodes;
    lineNodes.Create(3);

    // 5. Star: 1 central + 3 spokes (14-17, with 14 as central)
    Ptr<Node> starCentral = CreateObject<Node>();
    NodeContainer starSpokes;
    starSpokes.Create(3);
    NodeContainer starNodes;
    starNodes.Add(starCentral);
    starNodes.Add(starSpokes);

    // 6. Tree: 1 root + 2 children + 2 grandchildren (18-22, root=18)
    Ptr<Node> treeRoot = CreateObject<Node>();
    NodeContainer treeChildren;
    treeChildren.Create(2);
    NodeContainer treeGrandchildren;
    treeGrandchildren.Create(2);
    NodeContainer treeNodes;
    treeNodes.Add(treeRoot);
    treeNodes.Add(treeChildren);
    treeNodes.Add(treeGrandchildren);

    // Aggregate all nodes into one container for later use
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starNodes);
    allNodes.Add(treeNodes);

    // Internet stack
    OspfRoutingHelper ospfRoutingHelper;
    InternetStackHelper internet;
    internet.SetRoutingHelper(ospfRoutingHelper);
    internet.Install(allNodes);

    // PointToPoint and Csma helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(2000))); // 2us

    // Device containers for assigning addresses
    std::vector<NetDeviceContainer> deviceContainers;

    // ----------- Build Ring topology (0-3) -----------
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        uint32_t next = (i + 1) % ringNodes.GetN();
        NetDeviceContainer devs = p2p.Install(ringNodes.Get(i), ringNodes.Get(next));
        deviceContainers.push_back(devs);
    }

    // ----------- Build Mesh topology (4-6) -----------
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        for (uint32_t j = i + 1; j < meshNodes.GetN(); ++j)
        {
            NetDeviceContainer devs = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
            deviceContainers.push_back(devs);
        }
    }

    // ----------- Build Bus topology (7-10) -----------
    // Use Csma, then bridge device to mimic bus
    NetDeviceContainer csmaDevs = csma.Install(busNodes);
    deviceContainers.push_back(csmaDevs); // For address allocation

    // ----------- Build Line topology (11-13) ----------
    for (uint32_t i = 0; i < lineNodes.GetN() - 1; ++i)
    {
        NetDeviceContainer devs = p2p.Install(lineNodes.Get(i), lineNodes.Get(i + 1));
        deviceContainers.push_back(devs);
    }

    // ----------- Build Star topology (14-17) -----------
    for (uint32_t i = 1; i < starNodes.GetN(); ++i)
    {
        NetDeviceContainer devs = p2p.Install(starNodes.Get(0), starNodes.Get(i));
        deviceContainers.push_back(devs);
    }

    // ----------- Build Tree topology (18-22) -----------
    // root(0) - c1(1), root(0) - c2(2)
    NetDeviceContainer treeDev1 = p2p.Install(treeNodes.Get(0), treeNodes.Get(1));
    deviceContainers.push_back(treeDev1);
    NetDeviceContainer treeDev2 = p2p.Install(treeNodes.Get(0), treeNodes.Get(2));
    deviceContainers.push_back(treeDev2);
    // c1(1) - grandchild1(3)
    NetDeviceContainer treeDev3 = p2p.Install(treeNodes.Get(1), treeNodes.Get(3));
    deviceContainers.push_back(treeDev3);
    // c2(2) - grandchild2(4)
    NetDeviceContainer treeDev4 = p2p.Install(treeNodes.Get(2), treeNodes.Get(4));
    deviceContainers.push_back(treeDev4);

    // ----------- Interconnect topologies ---------------
    //
    // (Connect a few representative nodes between topologies for full connectivity)
    // e.g. ringNodes[0] <-> meshNodes[0]
    //      meshNodes[1] <-> starCentral
    //      starSpoke[1] <-> busNode[2]
    //      lineNode[0] <-> treeRoot
    //      lineNode[2] <-> ringNodes[2]
    //
    NetDeviceContainer inter1 = p2p.Install(ringNodes.Get(0), meshNodes.Get(0));
    deviceContainers.push_back(inter1);

    NetDeviceContainer inter2 = p2p.Install(meshNodes.Get(1), starNodes.Get(0));
    deviceContainers.push_back(inter2);

    NetDeviceContainer inter3 = p2p.Install(starNodes.Get(2), busNodes.Get(2));
    deviceContainers.push_back(inter3);

    NetDeviceContainer inter4 = p2p.Install(lineNodes.Get(0), treeNodes.Get(0));
    deviceContainers.push_back(inter4);

    NetDeviceContainer inter5 = p2p.Install(lineNodes.Get(2), ringNodes.Get(2));
    deviceContainers.push_back(inter5);

    // Assign addresses - allocate a subnet for each link
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;
    uint32_t netNum = 1;
    for (auto &dc : deviceContainers)
    {
        std::ostringstream subnet;
        subnet << "10." << netNum << ".0.0";
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        Ipv4InterfaceContainer ifc = address.Assign(dc);
        interfaceContainers.push_back(ifc);
        netNum++;
    }

    // Enable tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("hybrid-topology.tr"));
    p2p.EnablePcapAll("hybrid-topology");

    csma.EnableAsciiAll(ascii.CreateFileStream("hybrid-bus.tr"));
    csma.EnablePcapAll("hybrid-bus");

    // Enable NetAnim visualization
    AnimationInterface anim("hybrid-topology.xml");

    // Set custom node colors and positions for clarity in NetAnim
    // (Optional: enhancing animation visualization)
    // Topology layout sketch (rough):
    //  - Ring: upper left  (nodes 0-3)
    //  - Mesh: upper center (nodes 4-6)
    //  - Bus: upper right (nodes 7-10)
    //  - Line: lower left (nodes 11-13)
    //  - Star: lower center (nodes 14-17)
    //  - Tree: lower right (nodes 18-22)

    // Ring positions (0-3)
    anim.SetConstantPosition(ringNodes.Get(0), 10, 60);
    anim.SetConstantPosition(ringNodes.Get(1), 5, 65);
    anim.SetConstantPosition(ringNodes.Get(2), 10, 70);
    anim.SetConstantPosition(ringNodes.Get(3), 15, 65);

    // Mesh positions (4-6)
    anim.SetConstantPosition(meshNodes.Get(0), 35, 65);
    anim.SetConstantPosition(meshNodes.Get(1), 40, 70);
    anim.SetConstantPosition(meshNodes.Get(2), 45, 65);

    // Bus positions (7-10)
    anim.SetConstantPosition(busNodes.Get(0), 70, 68);
    anim.SetConstantPosition(busNodes.Get(1), 75, 67);
    anim.SetConstantPosition(busNodes.Get(2), 80, 66);
    anim.SetConstantPosition(busNodes.Get(3), 85, 65);

    // Line positions (11-13)
    anim.SetConstantPosition(lineNodes.Get(0), 15, 30);
    anim.SetConstantPosition(lineNodes.Get(1), 20, 25);
    anim.SetConstantPosition(lineNodes.Get(2), 25, 20);

    // Star positions (14-17)
    anim.SetConstantPosition(starNodes.Get(0), 50, 10); // center
    anim.SetConstantPosition(starNodes.Get(1), 45, 5);
    anim.SetConstantPosition(starNodes.Get(2), 55, 5);
    anim.SetConstantPosition(starNodes.Get(3), 50, 15);

    // Tree positions (18-22)
    anim.SetConstantPosition(treeNodes.Get(0), 80, 15); // root
    anim.SetConstantPosition(treeNodes.Get(1), 75, 10);
    anim.SetConstantPosition(treeNodes.Get(2), 85, 10);
    anim.SetConstantPosition(treeNodes.Get(3), 74, 5);
    anim.SetConstantPosition(treeNodes.Get(4), 86, 5);

    // OSPF routing is enabled via InternetStackHelper above
    // (No need for manual static routing)

    // (Optionally, set up an application for demo)
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(interfaceContainers[0].GetAddress(1), port));

    // UDP echo server running on first ring node
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(ringNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP echo client running on a bus node
    UdpEchoClientHelper echoClient(interfaceContainers[0].GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(busNodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}