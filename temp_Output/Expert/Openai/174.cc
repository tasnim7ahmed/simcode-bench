#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Simulation Attributes
    Time::SetResolution(Time::NS);

    // Containers for nodes in different topologies
    NodeContainer ringNodes;
    ringNodes.Create(4);

    NodeContainer meshNodes;
    meshNodes.Create(3);

    NodeContainer busNodes;
    busNodes.Create(3);

    NodeContainer lineNodes;
    lineNodes.Create(3);

    NodeContainer starCenter;
    starCenter.Create(1);
    NodeContainer starSpokes;
    starSpokes.Create(3);

    NodeContainer treeRoot;
    treeRoot.Create(1);
    NodeContainer treeLevel1;
    treeLevel1.Create(2);
    NodeContainer treeLeaves;
    treeLeaves.Create(4);

    // Create a master container for all nodes
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starCenter);
    allNodes.Add(starSpokes);
    allNodes.Add(treeRoot);
    allNodes.Add(treeLevel1);
    allNodes.Add(treeLeaves);

    // Install Internet stack with OSPF
    Ipv4ListRoutingHelper list;
    OspfRoutingHelper ospf;
    Ipv4StaticRoutingHelper staticRouting;
    list.Add(staticRouting, 0);
    list.Add(ospf, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(allNodes);

    // PointToPoint Links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Assign IP addresses
    std::vector<Ipv4InterfaceContainer> ifContainers;
    uint32_t subnetCount = 0;
    std::ostringstream subnet;
    auto NextSubnet = [&]() {
        subnet.str("");
        subnet << "10." << ((subnetCount >> 8) & 0xFF) << "." << (subnetCount & 0xFF) << ".0";
        ++subnetCount;
        return subnet.str();
    };

    // RING: Connect in a loop
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i)
    {
        NetDeviceContainer dev = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        Ipv4AddressHelper ipv4;
        ipv4.SetBase(NextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(ipv4.Assign(dev));
    }

    // MESH: Fully connect all mesh nodes
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        for (uint32_t j = i+1; j < meshNodes.GetN(); ++j)
        {
            NetDeviceContainer dev = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
            Ipv4AddressHelper ipv4;
            ipv4.SetBase(NextSubnet().c_str(), "255.255.255.0");
            ifContainers.push_back(ipv4.Assign(dev));
        }
    }

    // BUS: Chain and then connect all to a 'bus' (using a single intermediate switch node)
    Ptr<Node> busSwitch = CreateObject<Node>();
    internet.Install(busSwitch);
    for(uint32_t i=0; i<busNodes.GetN(); ++i)
    {
        NetDeviceContainer dev = p2p.Install(busNodes.Get(i), busSwitch);
        Ipv4AddressHelper ipv4;
        ipv4.SetBase(NextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(ipv4.Assign(dev));
    }

    // LINE: Connect node sequentially
    for(uint32_t i=0; i<lineNodes.GetN()-1; ++i)
    {
        NetDeviceContainer dev = p2p.Install(lineNodes.Get(i), lineNodes.Get(i+1));
        Ipv4AddressHelper ipv4;
        ipv4.SetBase(NextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(ipv4.Assign(dev));
    }

    // STAR: Center node connecting to all spokes
    for(uint32_t i=0; i<starSpokes.GetN(); ++i)
    {
        NetDeviceContainer dev = p2p.Install(starCenter.Get(0), starSpokes.Get(i));
        Ipv4AddressHelper ipv4;
        ipv4.SetBase(NextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(ipv4.Assign(dev));
    }

    // TREE: root -> level1, level1 -> leaves
    for(uint32_t i=0; i<treeLevel1.GetN(); ++i)
    {
        NetDeviceContainer dev = p2p.Install(treeRoot.Get(0), treeLevel1.Get(i));
        Ipv4AddressHelper ipv4;
        ipv4.SetBase(NextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(ipv4.Assign(dev));
    }
    for(uint32_t i=0; i<treeLevel1.GetN(); ++i)
    {
        NetDeviceContainer dev1 = p2p.Install(treeLevel1.Get(i), treeLeaves.Get(i*2));
        NetDeviceContainer dev2 = p2p.Install(treeLevel1.Get(i), treeLeaves.Get(i*2+1));
        Ipv4AddressHelper ipv4_1;
        ipv4_1.SetBase(NextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(ipv4_1.Assign(dev1));
        Ipv4AddressHelper ipv4_2;
        ipv4_2.SetBase(NextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(ipv4_2.Assign(dev2));
    }

    // INTERCONNECT: Join all topologies with at least one interlink each
    NetDeviceContainer devRingMesh = p2p.Install(ringNodes.Get(0), meshNodes.Get(0));
    Ipv4AddressHelper ipv4RingMesh;
    ipv4RingMesh.SetBase(NextSubnet().c_str(), "255.255.255.0");
    ifContainers.push_back(ipv4RingMesh.Assign(devRingMesh));

    NetDeviceContainer devMeshBus = p2p.Install(meshNodes.Get(1), busNodes.Get(0));
    Ipv4AddressHelper ipv4MeshBus;
    ipv4MeshBus.SetBase(NextSubnet().c_str(), "255.255.255.0");
    ifContainers.push_back(ipv4MeshBus.Assign(devMeshBus));

    NetDeviceContainer devBusLine = p2p.Install(busNodes.Get(2), lineNodes.Get(0));
    Ipv4AddressHelper ipv4BusLine;
    ipv4BusLine.SetBase(NextSubnet().c_str(), "255.255.255.0");
    ifContainers.push_back(ipv4BusLine.Assign(devBusLine));

    NetDeviceContainer devLineStar = p2p.Install(lineNodes.Get(2), starSpokes.Get(0));
    Ipv4AddressHelper ipv4LineStar;
    ipv4LineStar.SetBase(NextSubnet().c_str(), "255.255.255.0");
    ifContainers.push_back(ipv4LineStar.Assign(devLineStar));

    NetDeviceContainer devStarTree = p2p.Install(starSpokes.Get(2), treeRoot.Get(0));
    Ipv4AddressHelper ipv4StarTree;
    ipv4StarTree.SetBase(NextSubnet().c_str(), "255.255.255.0");
    ifContainers.push_back(ipv4StarTree.Assign(devStarTree));

    // NetAnim Node Positioning
    AnimationInterface anim("hybrid-topology.xml");

    // Ring
    double ringRadius = 20.0;
    for(uint32_t i=0;i<ringNodes.GetN();++i)
    {
        double angle = (2*M_PI*i)/ringNodes.GetN();
        double x = 25 + ringRadius * std::cos(angle);
        double y = 30 + ringRadius * std::sin(angle);
        anim.SetConstantPosition(ringNodes.Get(i), x, y);
    }

    // Mesh
    anim.SetConstantPosition(meshNodes.Get(0), 65, 10);
    anim.SetConstantPosition(meshNodes.Get(1), 85, 20);
    anim.SetConstantPosition(meshNodes.Get(2), 70, 25);

    // Bus Switch & Nodes
    anim.SetConstantPosition(busSwitch, 100, 38);
    for(uint32_t i=0;i<busNodes.GetN();++i)
    {
        anim.SetConstantPosition(busNodes.Get(i), 90+i*12, 38);
    }

    // Line
    for(uint32_t i=0;i<lineNodes.GetN();++i)
    {
        anim.SetConstantPosition(lineNodes.Get(i), 70+15*i, 55);
    }

    // Star
    anim.SetConstantPosition(starCenter.Get(0), 120, 70);
    for(uint32_t i=0;i<starSpokes.GetN();++i)
    {
        double angle=M_PI/2 + i*(2*M_PI/3); // 120 degrees apart
        double x = 120+15*std::cos(angle);
        double y = 70+15*std::sin(angle);
        anim.SetConstantPosition(starSpokes.Get(i), x, y);
    }

    // Tree
    anim.SetConstantPosition(treeRoot.Get(0), 150, 80);
    anim.SetConstantPosition(treeLevel1.Get(0), 140, 95);
    anim.SetConstantPosition(treeLevel1.Get(1), 160, 95);
    anim.SetConstantPosition(treeLeaves.Get(0), 135, 110);
    anim.SetConstantPosition(treeLeaves.Get(1), 145, 110);
    anim.SetConstantPosition(treeLeaves.Get(2), 155, 110);
    anim.SetConstantPosition(treeLeaves.Get(3), 165, 110);

    // A simple application for traffic visibility
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(treeLeaves.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));
    Ipv4Address remoteAddr = allNodes.Get(treeLeaves.Get(0)->GetId())->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();

    UdpEchoClientHelper echoClient(remoteAddr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(2));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    // Tracing
    p2p.EnablePcapAll("hybrid-topology");
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("hybrid-topology.tr"));

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}