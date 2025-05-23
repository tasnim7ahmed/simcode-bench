#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ospfv2-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int
main(int argc, char* argv[])
{
    LogComponentEnable("HybridTopologySimulation", LOG_LEVEL_INFO);
    // Optional: Enable OSPFv2 debug logs for routing table convergence observation
    // LogComponentEnable("Ospfv2Helper", LOG_LEVEL_DEBUG);
    // LogComponentEnable("Ospfv2Lsa", LOG_LEVEL_DEBUG);
    // LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_DEBUG);

    Time::SetResolution(Time::NS);

    // 1. Create Nodes for each topology segment and core
    NodeContainer coreNodes;
    coreNodes.Create(4); // CoreNode0, CoreNode1, CoreNode2, CoreNode3 (for Mesh)

    NodeContainer ringNodes;
    ringNodes.Create(4); // RingNode0, RingNode1, RingNode2, RingNode3

    NodeContainer lineNodes;
    lineNodes.Create(3); // LineNode0, LineNode1, LineNode2

    NodeContainer busNodes;
    busNodes.Create(4); // BusNode0, BusNode1, BusNode2, BusNode3 (P2P approximation of bus/long line)

    NodeContainer starNodes;
    starNodes.Create(4); // StarHubNode (starNodes.Get(0)), StarSpoke0, StarSpoke1, StarSpoke2

    NodeContainer treeNodes;
    treeNodes.Create(6); // TreeRoot (treeNodes.Get(0)), TreeBranch1 (1), TreeBranch2 (2),
                         // TreeLeaf1_1 (3), TreeLeaf1_2 (4), TreeLeaf2_1 (5)

    // Collect all nodes for InternetStack and OSPF installation
    NodeContainer allNodes;
    allNodes.Add(coreNodes);
    allNodes.Add(ringNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(busNodes);
    allNodes.Add(starNodes);
    allNodes.Add(treeNodes);

    // 2. Install Internet Stacks with OSPFv2 routing
    InternetStackHelper stack;
    Ospfv2Helper ospf;
    stack.SetRoutingHelper(ospf); // Set OSPF as the routing protocol
    stack.Install(allNodes);

    // 3. Create PointToPoint links and assign IP addresses
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.252"); // Start with /30 subnet

    // Core Mesh Topology (all-to-all for 4 nodes)
    NetDeviceContainer coreDevices;
    Ipv4InterfaceContainer coreInterfaces;
    // CoreNode0 to CoreNode1
    coreDevices = p2p.Install(coreNodes.Get(0), coreNodes.Get(1));
    coreInterfaces.Add(ipv4.Assign(coreDevices));
    ipv4.NewNetwork();
    // CoreNode0 to CoreNode2
    coreDevices = p2p.Install(coreNodes.Get(0), coreNodes.Get(2));
    coreInterfaces.Add(ipv4.Assign(coreDevices));
    ipv4.NewNetwork();
    // CoreNode0 to CoreNode3
    coreDevices = p2p.Install(coreNodes.Get(0), coreNodes.Get(3));
    coreInterfaces.Add(ipv4.Assign(coreDevices));
    ipv4.NewNetwork();
    // CoreNode1 to CoreNode2
    coreDevices = p2p.Install(coreNodes.Get(1), coreNodes.Get(2));
    coreInterfaces.Add(ipv4.Assign(coreDevices));
    ipv4.NewNetwork();
    // CoreNode1 to CoreNode3
    coreDevices = p2p.Install(coreNodes.Get(1), coreNodes.Get(3));
    coreInterfaces.Add(ipv4.Assign(coreDevices));
    ipv4.NewNetwork();
    // CoreNode2 to CoreNode3
    coreDevices = p2p.Install(coreNodes.Get(2), coreNodes.Get(3));
    coreInterfaces.Add(ipv4.Assign(coreDevices));
    ipv4.NewNetwork();

    // Ring Topology
    NetDeviceContainer ringDevices;
    Ipv4InterfaceContainer ringInterfaces;
    // RingNode0 <-> RingNode1
    ringDevices.Add(p2p.Install(ringNodes.Get(0), ringNodes.Get(1)));
    ringInterfaces.Add(ipv4.Assign(ringDevices.Get(ringDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // RingNode1 <-> RingNode2
    ringDevices.Add(p2p.Install(ringNodes.Get(1), ringNodes.Get(2)));
    ringInterfaces.Add(ipv4.Assign(ringDevices.Get(ringDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // RingNode2 <-> RingNode3
    ringDevices.Add(p2p.Install(ringNodes.Get(2), ringNodes.Get(3)));
    ringInterfaces.Add(ipv4.Assign(ringDevices.Get(ringDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // RingNode3 <-> RingNode0 (closes the ring)
    ringDevices.Add(p2p.Install(ringNodes.Get(3), ringNodes.Get(0)));
    ringInterfaces.Add(ipv4.Assign(ringDevices.Get(ringDevices.GetN() - 1)));
    ipv4.NewNetwork();

    // Line Topology
    NetDeviceContainer lineDevices;
    Ipv4InterfaceContainer lineInterfaces;
    // LineNode0 <-> LineNode1
    lineDevices.Add(p2p.Install(lineNodes.Get(0), lineNodes.Get(1)));
    lineInterfaces.Add(ipv4.Assign(lineDevices.Get(lineDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // LineNode1 <-> LineNode2
    lineDevices.Add(p2p.Install(lineNodes.Get(1), lineNodes.Get(2)));
    lineInterfaces.Add(ipv4.Assign(lineDevices.Get(lineDevices.GetN() - 1)));
    ipv4.NewNetwork();

    // Bus Topology (P2P approximated as a linear chain)
    NetDeviceContainer busDevices;
    Ipv4InterfaceContainer busInterfaces;
    // BusNode0 <-> BusNode1
    busDevices.Add(p2p.Install(busNodes.Get(0), busNodes.Get(1)));
    busInterfaces.Add(ipv4.Assign(busDevices.Get(busDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // BusNode1 <-> BusNode2
    busDevices.Add(p2p.Install(busNodes.Get(1), busNodes.Get(2)));
    busInterfaces.Add(ipv4.Assign(busDevices.Get(busDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // BusNode2 <-> BusNode3
    busDevices.Add(p2p.Install(busNodes.Get(2), busNodes.Get(3)));
    busInterfaces.Add(ipv4.Assign(busDevices.Get(busDevices.GetN() - 1)));
    ipv4.NewNetwork();

    // Star Topology
    NetDeviceContainer starDevices;
    Ipv4InterfaceContainer starInterfaces;
    // StarHubNode (starNodes.Get(0)) connected to spokes
    starDevices.Add(p2p.Install(starNodes.Get(0), starNodes.Get(1)));
    starInterfaces.Add(ipv4.Assign(starDevices.Get(starDevices.GetN() - 1)));
    ipv4.NewNetwork();
    starDevices.Add(p2p.Install(starNodes.Get(0), starNodes.Get(2)));
    starInterfaces.Add(ipv4.Assign(starDevices.Get(starDevices.GetN() - 1)));
    ipv4.NewNetwork();
    starDevices.Add(p2p.Install(starNodes.Get(0), starNodes.Get(3)));
    starInterfaces.Add(ipv4.Assign(starDevices.Get(starDevices.GetN() - 1)));
    ipv4.NewNetwork();

    // Tree Topology
    NetDeviceContainer treeDevices;
    Ipv4InterfaceContainer treeInterfaces;
    // TreeRoot (treeNodes.Get(0)) to branches
    treeDevices.Add(p2p.Install(treeNodes.Get(0), treeNodes.Get(1))); // Root to Branch1
    treeInterfaces.Add(ipv4.Assign(treeDevices.Get(treeDevices.GetN() - 1)));
    ipv4.NewNetwork();
    treeDevices.Add(p2p.Install(treeNodes.Get(0), treeNodes.Get(2))); // Root to Branch2
    treeInterfaces.Add(ipv4.Assign(treeDevices.Get(treeDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // Branch1 (treeNodes.Get(1)) to leaves
    treeDevices.Add(p2p.Install(treeNodes.Get(1), treeNodes.Get(3))); // Branch1 to Leaf1_1
    treeInterfaces.Add(ipv4.Assign(treeDevices.Get(treeDevices.GetN() - 1)));
    ipv4.NewNetwork();
    treeDevices.Add(p2p.Install(treeNodes.Get(1), treeNodes.Get(4))); // Branch1 to Leaf1_2
    treeInterfaces.Add(ipv4.Assign(treeDevices.Get(treeDevices.GetN() - 1)));
    ipv4.NewNetwork();
    // Branch2 (treeNodes.Get(2)) to leaf
    treeDevices.Add(p2p.Install(treeNodes.Get(2), treeNodes.Get(5))); // Branch2 to Leaf2_1
    treeInterfaces.Add(ipv4.Assign(treeDevices.Get(treeDevices.GetN() - 1)));
    ipv4.NewNetwork();

    // 4. Interconnect Topologies to the Core
    NetDeviceContainer interconnectDevices;
    Ipv4InterfaceContainer interconnectInterfaces;

    // CoreNode0 <-> RingNode0
    interconnectDevices = p2p.Install(coreNodes.Get(0), ringNodes.Get(0));
    interconnectInterfaces.Add(ipv4.Assign(interconnectDevices));
    ipv4.NewNetwork();

    // CoreNode1 <-> LineNode0
    interconnectDevices = p2p.Install(coreNodes.Get(1), lineNodes.Get(0));
    interconnectInterfaces.Add(ipv4.Assign(interconnectDevices));
    ipv4.NewNetwork();

    // CoreNode2 <-> BusNode0
    interconnectDevices = p2p.Install(coreNodes.Get(2), busNodes.Get(0));
    interconnectInterfaces.Add(ipv4.Assign(interconnectDevices));
    ipv4.NewNetwork();

    // CoreNode3 <-> StarHubNode (starNodes.Get(0))
    interconnectDevices = p2p.Install(coreNodes.Get(3), starNodes.Get(0));
    interconnectInterfaces.Add(ipv4.Assign(interconnectDevices));
    ipv4.NewNetwork();

    // CoreNode0 <-> TreeRoot (treeNodes.Get(0))
    interconnectDevices = p2p.Install(coreNodes.Get(0), treeNodes.Get(0));
    interconnectInterfaces.Add(ipv4.Assign(interconnectDevices));
    ipv4.NewNetwork();

    // 5. OSPF Configuration: All interfaces automatically configured with OSPF area 0.0.0.0
    //    because stack.SetRoutingHelper(ospf) was called before stack.Install().

    // 6. Enable tracing and NetAnim visualization
    p2p.EnablePcapAll("hybrid-topology", true); // PCAP tracing for all P2P links

    AnimationInterface anim("hybrid-topology.xml");
    // Set node positions for NetAnim visualization
    // Core nodes
    anim.SetConstantPosition(coreNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(coreNodes.Get(1), 50.0, 0.0);
    anim.SetConstantPosition(coreNodes.Get(2), 0.0, 50.0);
    anim.SetConstantPosition(coreNodes.Get(3), 50.0, 50.0);

    // Ring nodes (around CoreNode0)
    anim.SetConstantPosition(ringNodes.Get(0), -20.0, -20.0);
    anim.SetConstantPosition(ringNodes.Get(1), -40.0, -20.0);
    anim.SetConstantPosition(ringNodes.Get(2), -40.0, -40.0);
    anim.SetConstantPosition(ringNodes.Get(3), -20.0, -40.0);

    // Line nodes (extending from CoreNode1)
    anim.SetConstantPosition(lineNodes.Get(0), 70.0, 0.0);
    anim.SetConstantPosition(lineNodes.Get(1), 90.0, 0.0);
    anim.SetConstantPosition(lineNodes.Get(2), 110.0, 0.0);

    // Bus nodes (P2P chain, extending from CoreNode2)
    anim.SetConstantPosition(busNodes.Get(0), 0.0, 70.0);
    anim.SetConstantPosition(busNodes.Get(1), 0.0, 90.0);
    anim.SetConstantPosition(busNodes.Get(2), 0.0, 110.0);
    anim.SetConstantPosition(busNodes.Get(3), 0.0, 130.0);

    // Star nodes (around CoreNode3)
    anim.SetConstantPosition(starNodes.Get(0), 70.0, 70.0); // Hub
    anim.SetConstantPosition(starNodes.Get(1), 90.0, 70.0); // Spoke0
    anim.SetConstantPosition(starNodes.Get(2), 70.0, 90.0); // Spoke1
    anim.SetConstantPosition(starNodes.Get(3), 50.0, 90.0); // Spoke2

    // Tree nodes (extending from CoreNode0, but visually distinct)
    anim.SetConstantPosition(treeNodes.Get(0), -20.0, 20.0);  // Root
    anim.SetConstantPosition(treeNodes.Get(1), -40.0, 10.0);  // Branch1
    anim.SetConstantPosition(treeNodes.Get(2), -40.0, 30.0);  // Branch2
    anim.SetConstantPosition(treeNodes.Get(3), -60.0, 5.0);   // Leaf1_1
    anim.SetConstantPosition(treeNodes.Get(4), -60.0, 15.0);  // Leaf1_2
    anim.SetConstantPosition(treeNodes.Get(5), -60.0, 25.0);  // Leaf2_1

    // Add a simple UDP Echo application to demonstrate end-to-end connectivity
    // Server on StarSpoke0 (starNodes.Get(1))
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(starNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(9.0));

    // Client on TreeLeaf1_1 (treeNodes.Get(3)) to StarSpoke0
    // Get the IP address of StarSpoke0's interface on the link to StarHubNode.
    // The link between StarHubNode (starNodes.Get(0)) and StarSpoke0 (starNodes.Get(1))
    // is the first P2P link established for the Star topology (index 0 in starDevices).
    // The IP addresses are assigned to the nodes connected by this link.
    // starInterfaces.GetAddress(0) is StarHubNode's IP on this link.
    // starInterfaces.GetAddress(1) is StarSpoke0's IP on this link.
    Ipv4Address starSpoke0Ip = starInterfaces.GetAddress(1);

    UdpEchoClientHelper echoClient(starSpoke0Ip, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(treeNodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(8.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}