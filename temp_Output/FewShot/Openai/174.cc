#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable Logging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create all nodes
    // 5 for ring, 4 for mesh, 3 for bus, 3 for line, 1 central+3 spokes for star, 7 for tree
    // Ensure unique nodes, but to keep them visually separated, create different node containers
    NodeContainer ringNodes;
    ringNodes.Create(5);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(3);

    NodeContainer lineNodes;
    lineNodes.Create(3);

    NodeContainer starHub;
    starHub.Create(1);
    NodeContainer starSpokes;
    starSpokes.Create(3);

    NodeContainer treeNodes;
    treeNodes.Create(7); // Root(0), L1(1,2), L2(3,4,5,6), standard binary tree 3 levels

    // Create a master node list for InternetStack installation
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starHub);
    allNodes.Add(starSpokes);
    allNodes.Add(treeNodes);

    // Install Internet stack with OSPF routing on all nodes
    OspfRoutingHelper ospf;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospf, 10);
    InternetStackHelper internet;
    internet.SetIpv4StackInstall(true);
    internet.SetRoutingHelper(listRouting);
    internet.Install(allNodes);

    // Helper for point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<Ipv4InterfaceContainer> ifContainers;

    uint32_t subnetIdx = 0;
    char netBaseBuf[16];

    // Function to assign subnet
    auto getNextSubnet = [&subnetIdx, &netBaseBuf]() {
        uint32_t a = 10 + subnetIdx / (256 * 256);
        uint32_t b = (subnetIdx / 256) % 256;
        uint32_t c = subnetIdx % 256;
        snprintf(netBaseBuf, sizeof(netBaseBuf), "%u.%u.%u.0", a, b, c);
        subnetIdx++;
        return std::string(netBaseBuf);
    };

    // Ring Topology: 5 nodes in a ring
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i) {
        NodeContainer pair(ringNodes.Get(i), ringNodes.Get((i+1)%ringNodes.GetN()));
        NetDeviceContainer devs = p2p.Install(pair);
        Ipv4AddressHelper addr;
        addr.SetBase(getNextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(addr.Assign(devs));
        deviceContainers.push_back(devs);
    }

    // Mesh Topology: fully connect meshNodes
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        for (uint32_t j = i+1; j < meshNodes.GetN(); ++j) {
            NodeContainer pair(meshNodes.Get(i), meshNodes.Get(j));
            NetDeviceContainer devs = p2p.Install(pair);
            Ipv4AddressHelper addr;
            addr.SetBase(getNextSubnet().c_str(), "255.255.255.0");
            ifContainers.push_back(addr.Assign(devs));
            deviceContainers.push_back(devs);
        }
    }

    // Bus Topology: Linear connect busNodes, but connect shared with an extra link (simulate shared medium)
    for (uint32_t i = 0; i < busNodes.GetN()-1; ++i) {
        NodeContainer pair(busNodes.Get(i), busNodes.Get(i+1));
        NetDeviceContainer devs = p2p.Install(pair);
        Ipv4AddressHelper addr;
        addr.SetBase(getNextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(addr.Assign(devs));
        deviceContainers.push_back(devs);
    }

    // Line Topology: chain the line nodes
    for (uint32_t i = 0; i < lineNodes.GetN()-1; ++i) {
        NodeContainer pair(lineNodes.Get(i), lineNodes.Get(i+1));
        NetDeviceContainer devs = p2p.Install(pair);
        Ipv4AddressHelper addr;
        addr.SetBase(getNextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(addr.Assign(devs));
        deviceContainers.push_back(devs);
    }

    // Star Topology: hub node connected to each spoke
    for (uint32_t i = 0; i < starSpokes.GetN(); ++i) {
        NodeContainer pair(starHub.Get(0), starSpokes.Get(i));
        NetDeviceContainer devs = p2p.Install(pair);
        Ipv4AddressHelper addr;
        addr.SetBase(getNextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(addr.Assign(devs));
        deviceContainers.push_back(devs);
    }

    // Tree Topology: (binary tree) root=0, children 1 (L=left), 2 (R=right), 3,4 (children of 1), 5,6 (children of 2)
    NetDeviceContainer treeDev;
    // root to L1 children
    treeDev = p2p.Install(NodeContainer(treeNodes.Get(0), treeNodes.Get(1)));
    {Ipv4AddressHelper addr; addr.SetBase(getNextSubnet().c_str(), "255.255.255.0"); ifContainers.push_back(addr.Assign(treeDev)); deviceContainers.push_back(treeDev);}
    treeDev = p2p.Install(NodeContainer(treeNodes.Get(0), treeNodes.Get(2)));
    {Ipv4AddressHelper addr; addr.SetBase(getNextSubnet().c_str(), "255.255.255.0"); ifContainers.push_back(addr.Assign(treeDev)); deviceContainers.push_back(treeDev);}
    // L1 children to their L2
    treeDev = p2p.Install(NodeContainer(treeNodes.Get(1), treeNodes.Get(3)));
    {Ipv4AddressHelper addr; addr.SetBase(getNextSubnet().c_str(), "255.255.255.0"); ifContainers.push_back(addr.Assign(treeDev)); deviceContainers.push_back(treeDev);}
    treeDev = p2p.Install(NodeContainer(treeNodes.Get(1), treeNodes.Get(4)));
    {Ipv4AddressHelper addr; addr.SetBase(getNextSubnet().c_str(), "255.255.255.0"); ifContainers.push_back(addr.Assign(treeDev)); deviceContainers.push_back(treeDev);}
    treeDev = p2p.Install(NodeContainer(treeNodes.Get(2), treeNodes.Get(5)));
    {Ipv4AddressHelper addr; addr.SetBase(getNextSubnet().c_str(), "255.255.255.0"); ifContainers.push_back(addr.Assign(treeDev)); deviceContainers.push_back(treeDev);}
    treeDev = p2p.Install(NodeContainer(treeNodes.Get(2), treeNodes.Get(6)));
    {Ipv4AddressHelper addr; addr.SetBase(getNextSubnet().c_str(), "255.255.255.0"); ifContainers.push_back(addr.Assign(treeDev)); deviceContainers.push_back(treeDev);}

    // Hybrid Interconnections: Connect one node from each topology to form a backbone
    NodeContainer hybridBackbone;
    hybridBackbone.Add(ringNodes.Get(0));
    hybridBackbone.Add(meshNodes.Get(0));
    hybridBackbone.Add(busNodes.Get(0));
    hybridBackbone.Add(lineNodes.Get(0));
    hybridBackbone.Add(starHub.Get(0));
    hybridBackbone.Add(treeNodes.Get(0));
    for (uint32_t i = 0; i < hybridBackbone.GetN()-1; ++i) {
        NodeContainer pair(hybridBackbone.Get(i), hybridBackbone.Get(i+1));
        NetDeviceContainer devs = p2p.Install(pair);
        Ipv4AddressHelper addr;
        addr.SetBase(getNextSubnet().c_str(), "255.255.255.0");
        ifContainers.push_back(addr.Assign(devs));
        deviceContainers.push_back(devs);
    }

    // Enable tracing
    AsciiTraceHelper ascii;
    for (size_t i = 0; i < deviceContainers.size(); ++i) {
        std::ostringstream oss;
        oss << "hybrid-topology-link-" << i << ".tr";
        p2p.EnableAsciiAll(ascii.CreateFileStream(oss.str()));
        std::ostringstream pcaposs;
        pcaposs << "hybrid-topology-link-" << i;
        p2p.EnablePcapAll(pcaposs.str(), false);
    }

    // Applications (few ICMP Ping to probe connectivity)
    uint16_t port = 8000;
    // Install PacketSink on meshNodes.Get(1) as example
    Address sinkAddr(InetSocketAddress(ifContainers[1].GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(meshNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(20.0));

    // Install OnOffApplication (TCP) from busNodes.Get(2) to meshNodes.Get(1)
    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
    onoff.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(18.0)));
    ApplicationContainer clientApp = onoff.Install(busNodes.Get(2));

    // NetAnim configuration
    AnimationInterface anim("hybrid-topology.xml");

    // Layout for netanim (set positions for clarity)
    double x = 0.0, y = 0.0;
    // Ring
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i) {
        double angle = 2*M_PI*i/ringNodes.GetN();
        x = 30 + 10*cos(angle);
        y = 60 + 10*sin(angle);
        anim.SetConstantPosition(ringNodes.Get(i), x, y);
    }
    // Mesh
    anim.SetConstantPosition(meshNodes.Get(0), 70, 55);
    anim.SetConstantPosition(meshNodes.Get(1), 80, 65);
    anim.SetConstantPosition(meshNodes.Get(2), 90, 55);
    anim.SetConstantPosition(meshNodes.Get(3), 80, 45);
    // Bus
    for (uint32_t i = 0; i < busNodes.GetN(); ++i) {
        anim.SetConstantPosition(busNodes.Get(i), 30 + i*10, 20);
    }
    // Line
    for (uint32_t i = 0; i < lineNodes.GetN(); ++i) {
        anim.SetConstantPosition(lineNodes.Get(i), 5+i*7, 35);
    }
    // Star
    anim.SetConstantPosition(starHub.Get(0), 60, 80);
    anim.SetConstantPosition(starSpokes.Get(0), 70, 90);
    anim.SetConstantPosition(starSpokes.Get(1), 50, 90);
    anim.SetConstantPosition(starSpokes.Get(2), 60, 100);
    // Tree (draw shade like a tree)
    anim.SetConstantPosition(treeNodes.Get(0), 120, 80); // root
    anim.SetConstantPosition(treeNodes.Get(1), 110, 70);
    anim.SetConstantPosition(treeNodes.Get(2), 130, 70);
    anim.SetConstantPosition(treeNodes.Get(3), 105, 60);
    anim.SetConstantPosition(treeNodes.Get(4), 115, 60);
    anim.SetConstantPosition(treeNodes.Get(5), 125, 60);
    anim.SetConstantPosition(treeNodes.Get(6), 135, 60);

    // Start simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}