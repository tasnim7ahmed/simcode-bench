#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Ring topology: 4 nodes connected in a ring
    NodeContainer ringNodes;
    ringNodes.Create(4);

    // Mesh topology: 3 nodes fully meshed
    NodeContainer meshNodes;
    meshNodes.Create(3);

    // Bus topology: 3 nodes connected in a line (bus simulated by chaining)
    NodeContainer busNodes;
    busNodes.Create(3);

    // Line topology: 2 nodes connected linearly
    NodeContainer lineNodes;
    lineNodes.Create(2);

    // Star topology: 1 hub (starNodes.Get(0)), 4 spokes (1-4)
    NodeContainer starNodes;
    starNodes.Create(5);

    // Tree topology: root, left, right, two children for each (7 nodes)
    NodeContainer treeNodes;
    treeNodes.Create(7);

    // Container to hold all nodes
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starNodes);
    allNodes.Add(treeNodes);

    // Install Internet stack with OSPF routing
    OspfHelper ospfRouting;
    Ipv4ListRoutingHelper list;
    list.Add(ospfRouting, 10);
    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(allNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devs;
    std::vector<Ipv4InterfaceContainer> ifaces;
    Ipv4AddressHelper address;
    uint32_t netId = 1;

    // Ring topology connections (0-1-2-3-0)
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n1 = i;
        uint32_t n2 = (i+1)%4;
        NetDeviceContainer d = p2p.Install(ringNodes.Get(n1), ringNodes.Get(n2));
        address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
        ifaces.push_back(address.Assign(d));
        ++netId;
    }

    // Mesh topology connections (full mesh: 0-1,0-2,1-2)
    for (uint32_t i = 0; i < 3; ++i) {
        for (uint32_t j = i+1; j < 3; ++j) {
            NetDeviceContainer d = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
            address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
            ifaces.push_back(address.Assign(d));
            ++netId;
        }
    }

    // Bus topology (chained 0-1-2)
    for (uint32_t i = 0; i < 2; ++i) {
        NetDeviceContainer d = p2p.Install(busNodes.Get(i), busNodes.Get(i+1));
        address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
        ifaces.push_back(address.Assign(d));
        ++netId;
    }

    // Line topology (0-1)
    NetDeviceContainer lineLink = p2p.Install(lineNodes.Get(0), lineNodes.Get(1));
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(lineLink));
    ++netId;

    // Star topology (hub is node 0, spokes are 1-4)
    for (uint32_t i = 1; i < 5; ++i) {
        NetDeviceContainer d = p2p.Install(starNodes.Get(0), starNodes.Get(i));
        address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
        ifaces.push_back(address.Assign(d));
        ++netId;
    }

    // Tree topology (binary): 0=root, 1=left, 2=right, 3/4 children of 1, 5/6 children of 2
    NetDeviceContainer treeLink1 = p2p.Install(treeNodes.Get(0), treeNodes.Get(1)); // root-left
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(treeLink1));
    ++netId;

    NetDeviceContainer treeLink2 = p2p.Install(treeNodes.Get(0), treeNodes.Get(2)); // root-right
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(treeLink2));
    ++netId;

    NetDeviceContainer treeLink3 = p2p.Install(treeNodes.Get(1), treeNodes.Get(3)); // left-left
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(treeLink3));
    ++netId;

    NetDeviceContainer treeLink4 = p2p.Install(treeNodes.Get(1), treeNodes.Get(4)); // left-right
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(treeLink4));
    ++netId;

    NetDeviceContainer treeLink5 = p2p.Install(treeNodes.Get(2), treeNodes.Get(5)); // right-left
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(treeLink5));
    ++netId;

    NetDeviceContainer treeLink6 = p2p.Install(treeNodes.Get(2), treeNodes.Get(6)); // right-right
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(treeLink6));
    ++netId;

    // Interconnect each topology by bridging one node to the next topology's node (hybridization)
    // 0) ring[0] <-> mesh[0]
    NetDeviceContainer hybrid1 = p2p.Install(ringNodes.Get(0), meshNodes.Get(0));
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(hybrid1));
    ++netId;

    // 1) mesh[1] <-> bus[0]
    NetDeviceContainer hybrid2 = p2p.Install(meshNodes.Get(1), busNodes.Get(0));
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(hybrid2));
    ++netId;

    // 2) bus[2] <-> line[0]
    NetDeviceContainer hybrid3 = p2p.Install(busNodes.Get(2), lineNodes.Get(0));
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(hybrid3));
    ++netId;

    // 3) line[1] <-> star[0]
    NetDeviceContainer hybrid4 = p2p.Install(lineNodes.Get(1), starNodes.Get(0));
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(hybrid4));
    ++netId;

    // 4) star[4] <-> tree[0]
    NetDeviceContainer hybrid5 = p2p.Install(starNodes.Get(4), treeNodes.Get(0));
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(hybrid5));
    ++netId;

    // 5) tree[6] <-> ring[1]
    NetDeviceContainer hybrid6 = p2p.Install(treeNodes.Get(6), ringNodes.Get(1));
    address.SetBase(Ipv4Address("10." + std::to_string(netId) + ".1.0"), "255.255.255.0");
    ifaces.push_back(address.Assign(hybrid6));
    ++netId;

    // Enable pcap tracing for each point-to-point device
    for (auto &d : ifaces) {
        for (uint32_t i = 0; i < d.GetN(); ++i) {
            Ptr<NetDevice> dev = d.Get(i).first->GetDevice(0);
            Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice>(dev);
            if (p2pDev) {
                p2p.EnablePcap(p2pDev->GetNode()->GetName(), p2pDev, true, true);
            }
        }
    }

    // NetAnim: set node positions for visualization
    AnimationInterface anim("hybrid-topology.xml");
    uint32_t n = 0;
    // Arrange topologies spatially: (basic layout)
    // Ring: Circle at (100,200)
    double theta = 0;
    double radius = 30.0;
    for (uint32_t i = 0; i < 4; ++i, ++n) {
        double x = 100 + radius * std::cos(theta);
        double y = 200 + radius * std::sin(theta);
        anim.SetConstantPosition(ringNodes.Get(i), x, y);
        theta += 2.0 * M_PI / 4.0;
    }
    // Mesh: Triangle at (200,200)
    double meshX[3] = {230, 210, 250};
    double meshY[3] = {230, 250, 250};
    for (uint32_t i = 0; i < 3; ++i, ++n) {
        anim.SetConstantPosition(meshNodes.Get(i), meshX[i], meshY[i]);
    }
    // Bus: Line at (300,180) to (360,180)
    for (uint32_t i = 0; i < 3; ++i, ++n) {
        anim.SetConstantPosition(busNodes.Get(i), 300 + i*30, 180);
    }
    // Line: (380,160)-(410,160)
    for (uint32_t i = 0; i < 2; ++i, ++n) {
        anim.SetConstantPosition(lineNodes.Get(i), 380 + i*30, 160);
    }
    // Star: (460,220) as hub, spokes around it
    anim.SetConstantPosition(starNodes.Get(0), 460, 220); ++n;
    anim.SetConstantPosition(starNodes.Get(1), 490, 220); ++n;
    anim.SetConstantPosition(starNodes.Get(2), 460, 250); ++n;
    anim.SetConstantPosition(starNodes.Get(3), 430, 220); ++n;
    anim.SetConstantPosition(starNodes.Get(4), 460, 190); ++n;
    // Tree: (540,230) root, (510,235),(570,235) as child, next level further out
    anim.SetConstantPosition(treeNodes.Get(0), 540, 230); ++n;   // root
    anim.SetConstantPosition(treeNodes.Get(1), 510, 245); ++n;   // left
    anim.SetConstantPosition(treeNodes.Get(2), 570, 245); ++n;   // right
    anim.SetConstantPosition(treeNodes.Get(3), 490, 260); ++n;   // left-left
    anim.SetConstantPosition(treeNodes.Get(4), 530, 260); ++n;   // left-right
    anim.SetConstantPosition(treeNodes.Get(5), 550, 260); ++n;   // right-left
    anim.SetConstantPosition(treeNodes.Get(6), 590, 260); ++n;   // right-right

    anim.UpdateNodeDescription();
    anim.UpdateNodeColor();

    // Simulation time
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}