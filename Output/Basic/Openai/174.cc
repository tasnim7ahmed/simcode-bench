#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-ospf-routing-helper.h"
#include <vector>
#include <map>

using namespace ns3;

int 
main(int argc, char *argv[])
{
    // Logging
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // 1. Create base nodes for each topology
    // Ring: 4 nodes
    NodeContainer ringNodes;
    ringNodes.Create(4);

    // Mesh: 4 nodes
    NodeContainer meshNodes;
    meshNodes.Create(4);

    // Bus: 4 nodes
    NodeContainer busNodes;
    busNodes.Create(4);

    // Line: 4 nodes
    NodeContainer lineNodes;
    lineNodes.Create(4);

    // Star: center + 3 spokes
    NodeContainer starCenter;
    starCenter.Create(1);
    NodeContainer starSpokes;
    starSpokes.Create(3);

    // Tree: root + 2 level-one + 4 level-two
    NodeContainer treeRoot;
    treeRoot.Create(1);
    NodeContainer treeLevel1;
    treeLevel1.Create(2);
    NodeContainer treeLevel2;
    treeLevel2.Create(4);

    // 2. Interconnect these topologies into a hybrid
    // We'll connect: 
    // ring[0] <-> mesh[0]
    // mesh[1] <-> bus[0]
    // bus[1] <-> line[0]
    // line[1] <-> starCenter[0]
    // starSpokes[0] <-> treeRoot[0]
    // treeLevel2[3] <-> ring[2]
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> allDevs;
    std::vector<Ipv4InterfaceContainer> allIfaces;
    int subnetCount = 0;
    std::map<uint32_t, Ipv4Address> nodeIpBase; // for NetAnim

    // For ip assignment
    auto NextSubnet = [&subnetCount]() -> std::string {
        // 10.1.X.0/24 subnets
        std::ostringstream oss;
        oss << "10.1." << subnetCount << ".0";
        subnetCount++;
        return oss.str();
    };

    // 3. Point-to-point links within each topology

    // Ring
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        uint32_t next = (i+1)%ringNodes.GetN();
        NodeContainer nc;
        nc.Add(ringNodes.Get(i));
        nc.Add(ringNodes.Get(next));
        NetDeviceContainer nd = p2p.Install(nc);
        allDevs.push_back(nd);
    }

    // Mesh
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        for (uint32_t j = i+1; j < meshNodes.GetN(); ++j)
        {
            NodeContainer nc;
            nc.Add(meshNodes.Get(i));
            nc.Add(meshNodes.Get(j));
            NetDeviceContainer nd = p2p.Install(nc);
            allDevs.push_back(nd);
        }
    }

    // Bus (implemented as sequential links for simplicity)
    for (uint32_t i = 0; i < busNodes.GetN() - 1; ++i)
    {
        NodeContainer nc;
        nc.Add(busNodes.Get(i));
        nc.Add(busNodes.Get(i+1));
        NetDeviceContainer nd = p2p.Install(nc);
        allDevs.push_back(nd);
    }

    // Line
    for (uint32_t i = 0; i < lineNodes.GetN() - 1; ++i)
    {
        NodeContainer nc;
        nc.Add(lineNodes.Get(i));
        nc.Add(lineNodes.Get(i+1));
        NetDeviceContainer nd = p2p.Install(nc);
        allDevs.push_back(nd);
    }

    // Star
    for (uint32_t i = 0; i < starSpokes.GetN(); ++i)
    {
        NodeContainer nc;
        nc.Add(starCenter.Get(0));
        nc.Add(starSpokes.Get(i));
        NetDeviceContainer nd = p2p.Install(nc);
        allDevs.push_back(nd);
    }

    // Tree
    // root <-> level 1
    for (uint32_t i = 0; i < treeLevel1.GetN(); ++i)
    {
        NodeContainer nc;
        nc.Add(treeRoot.Get(0));
        nc.Add(treeLevel1.Get(i));
        NetDeviceContainer nd = p2p.Install(nc);
        allDevs.push_back(nd);
    }
    // level 1 <-> level 2 (2 children per parent)
    for (uint32_t i = 0; i < treeLevel1.GetN(); ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            NodeContainer nc;
            nc.Add(treeLevel1.Get(i));
            nc.Add(treeLevel2.Get(i*2 + j));
            NetDeviceContainer nd = p2p.Install(nc);
            allDevs.push_back(nd);
        }
    }

    // 4. Interconnect topologies
    std::vector<NodeContainer> interTopo;
    // ring[0] <-> mesh[0]
    NodeContainer nc1;
    nc1.Add(ringNodes.Get(0));
    nc1.Add(meshNodes.Get(0));
    interTopo.push_back(nc1);

    // mesh[1] <-> bus[0]
    NodeContainer nc2;
    nc2.Add(meshNodes.Get(1));
    nc2.Add(busNodes.Get(0));
    interTopo.push_back(nc2);

    // bus[1] <-> line[0]
    NodeContainer nc3;
    nc3.Add(busNodes.Get(1));
    nc3.Add(lineNodes.Get(0));
    interTopo.push_back(nc3);

    // line[1] <-> starCenter[0]
    NodeContainer nc4;
    nc4.Add(lineNodes.Get(1));
    nc4.Add(starCenter.Get(0));
    interTopo.push_back(nc4);

    // starSpokes[0] <-> treeRoot[0]
    NodeContainer nc5;
    nc5.Add(starSpokes.Get(0));
    nc5.Add(treeRoot.Get(0));
    interTopo.push_back(nc5);

    // treeLevel2[3] <-> ring[2]
    NodeContainer nc6;
    nc6.Add(treeLevel2.Get(3));
    nc6.Add(ringNodes.Get(2));
    interTopo.push_back(nc6);

    for (auto& nc : interTopo)
    {
        NetDeviceContainer nd = p2p.Install(nc);
        allDevs.push_back(nd);
    }

    // 5. Aggregate all nodes
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starCenter);
    allNodes.Add(starSpokes);
    allNodes.Add(treeRoot);
    allNodes.Add(treeLevel1);
    allNodes.Add(treeLevel2);

    // 6. Install Internet stack with OSPF
    OspfRoutingHelper ospf;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospf);
    stack.Install(allNodes);

    // 7. IPv4 address assignment, one subnet per link
    Ipv4AddressHelper ipv4;
    for (auto& nd : allDevs)
    {
        std::string base = NextSubnet();
        ipv4.SetBase(base.c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = ipv4.Assign(nd);
        allIfaces.push_back(iface);
    }

    // 8. Enable tracing
    AsciiTraceHelper ascii;
    p2p.EnablePcapAll("hybrid-topology");
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("hybrid-trace.tr");
    for (auto& devs : allDevs)
        p2p.EnableAsciiAll(stream);

    // 9. NetAnim: assign positions for relative clarity
    AnimationInterface anim("hybrid-topology.xml");

    // Placement variables
    double x_gap = 70, y_gap = 70;
    double start_x = 100, start_y = 100;
    double offset = 0;

    // Ring (circle)
    double RingR = 60.0, RingCX = 200, RingCY = 200;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        double angle = 2 * M_PI * i / ringNodes.GetN();
        double x = RingCX + RingR * cos(angle);
        double y = RingCY + RingR * sin(angle);
        anim.SetConstantPosition(ringNodes.Get(i), x, y);
    }

    // Mesh (square area)
    double MeshX = 350, MeshY = 200, MeshS = 60;
    anim.SetConstantPosition(meshNodes.Get(0), MeshX, MeshY);
    anim.SetConstantPosition(meshNodes.Get(1), MeshX+MeshS, MeshY);
    anim.SetConstantPosition(meshNodes.Get(2), MeshX, MeshY+MeshS);
    anim.SetConstantPosition(meshNodes.Get(3), MeshX+MeshS, MeshY+MeshS);

    // Bus (horizontal line)
    for (uint32_t i = 0; i < busNodes.GetN(); ++i)
        anim.SetConstantPosition(busNodes.Get(i), 200 + i*x_gap, 350);

    // Line (vertical)
    for (uint32_t i = 0; i < lineNodes.GetN(); ++i)
        anim.SetConstantPosition(lineNodes.Get(i), 350, 350 + i*y_gap);

    // Star (center + spokes)
    anim.SetConstantPosition(starCenter.Get(0), 350, 600);
    anim.SetConstantPosition(starSpokes.Get(0), 350, 730);
    anim.SetConstantPosition(starSpokes.Get(1), 280, 680);
    anim.SetConstantPosition(starSpokes.Get(2), 420, 680);

    // Tree (downward, hierarchical)
    anim.SetConstantPosition(treeRoot.Get(0), 600, 200);
    anim.SetConstantPosition(treeLevel1.Get(0), 570, 300);
    anim.SetConstantPosition(treeLevel1.Get(1), 630, 300);
    anim.SetConstantPosition(treeLevel2.Get(0), 560, 400);
    anim.SetConstantPosition(treeLevel2.Get(1), 580, 400);
    anim.SetConstantPosition(treeLevel2.Get(2), 620, 400);
    anim.SetConstantPosition(treeLevel2.Get(3), 640, 400);

    // 10. Run some applications (optional, quick echo between two far nodes)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(Ipv4Address("10.1.0.2"), port); // The mesh node's first iface (guaranteed by enumeration)
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(treeLevel2.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}