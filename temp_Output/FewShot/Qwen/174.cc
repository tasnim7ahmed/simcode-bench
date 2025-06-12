#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-ospf-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable OSPF logging if needed for debugging
    // LogComponentEnable("Ipv4OspfRouting", LOG_LEVEL_INFO);

    // Create nodes for different topologies
    NodeContainer ringNodes;
    ringNodes.Create(4);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(5);

    NodeContainer lineNodes;
    lineNodes.Create(3);

    NodeContainer starHub;
    starHub.Create(1);
    NodeContainer starSpokes;
    starSpokes.Create(5);

    NodeContainer treeLevel1;
    treeLevel1.Create(1);
    NodeContainer treeLevel2;
    treeLevel2.Create(2);
    NodeContainer treeLevel3;
    treeLevel3.Create(4);

    // Combine all nodes into a global container
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starHub);
    allNodes.Add(starSpokes);
    allNodes.Add(treeLevel1);
    allNodes.Add(treeLevel2);
    allNodes.Add(treeLevel3);

    // Set up point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install Internet stack with dynamic routing (OSPF)
    InternetStackHelper stack;
    Ipv4OspfRoutingHelper ospfRouting;
    Ipv4ListRoutingHelper list;
    list.Add(ospfRouting, 0);
    stack.SetRoutingHelper(list);  // has to be set before installing
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("192.168.0.0", "255.255.0.0");

    NetDeviceContainer devices;

    // Ring topology: connect each node to the next and last to first
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i) {
        uint32_t j = (i + 1) % ringNodes.GetN();
        NetDeviceContainer dev = p2p.Install(NodeContainer(ringNodes.Get(i), ringNodes.Get(j)));
        devices.Add(dev);
    }

    // Mesh topology: fully connected between mesh nodes
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        for (uint32_t j = i + 1; j < meshNodes.GetN(); ++j) {
            NetDeviceContainer dev = p2p.Install(NodeContainer(meshNodes.Get(i), meshNodes.Get(j)));
            devices.Add(dev);
        }
    }

    // Bus topology: linear connection of bus nodes
    for (uint32_t i = 0; i < busNodes.GetN() - 1; ++i) {
        NetDeviceContainer dev = p2p.Install(NodeContainer(busNodes.Get(i), busNodes.Get(i + 1)));
        devices.Add(dev);
    }

    // Line topology: simple chain
    for (uint32_t i = 0; i < lineNodes.GetN() - 1; ++i) {
        NetDeviceContainer dev = p2p.Install(NodeContainer(lineNodes.Get(i), lineNodes.Get(i + 1)));
        devices.Add(dev);
    }

    // Star topology: spokes connected to hub
    for (uint32_t i = 0; i < starSpokes.GetN(); ++i) {
        NetDeviceContainer dev = p2p.Install(NodeContainer(starHub.Get(0), starSpokes.Get(i)));
        devices.Add(dev);
    }

    // Tree topology: level connections
    // Level 1 -> Level 2
    for (uint32_t i = 0; i < treeLevel2.GetN(); ++i) {
        NetDeviceContainer dev = p2p.Install(NodeContainer(treeLevel1.Get(0), treeLevel2.Get(i)));
        devices.Add(dev);
    }
    // Level 2 -> Level 3
    uint32_t count = 0;
    for (uint32_t i = 0; i < treeLevel2.GetN(); ++i) {
        for (uint32_t j = 0; j < 2 && count < treeLevel3.GetN(); ++j) {
            NetDeviceContainer dev = p2p.Install(NodeContainer(treeLevel2.Get(i), treeLevel3.Get(count++)));
            devices.Add(dev);
        }
    }

    // Assign IP addresses to all devices
    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        interfaces.Add(ip.Assign(devices.Get(i)));
    }

    // Setup NetAnim for visualization
    AnimationInterface anim("hybrid-topology.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routing-table-writer.xml", Seconds(0), Seconds(20), Seconds(0.25));

    // Add nodes to animation with positions (simplified layout)
    for (uint32_t i = 0; i < allNodes.GetN(); ++i) {
        anim.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        Vector position(i * 20, 0, 0); // Simple linear placement for illustration
        allNodes.Get(i)->GetObject<MobilityModel>()->SetPosition(position);
    }

    // Run simulation
    Simulator::Stop(Seconds(20));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}