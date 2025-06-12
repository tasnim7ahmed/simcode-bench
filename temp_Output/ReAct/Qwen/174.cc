#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for different topologies
    NodeContainer ringNodes;
    ringNodes.Create(4); // 4-node ring

    NodeContainer meshNodes;
    meshNodes.Create(4); // 4-node fully connected mesh

    NodeContainer busNodes;
    busNodes.Create(5); // 5-node bus (linear backbone)

    NodeContainer lineNodes;
    lineNodes.Create(4); // 4-node line

    NodeContainer starHub;
    starHub.Create(1); // Central hub of star
    NodeContainer starSpokes;
    starSpokes.Create(5); // 5 spokes in star

    NodeContainer treeRoot;
    treeRoot.Create(1);
    NodeContainer treeLevel1;
    treeLevel1.Create(2);
    NodeContainer treeLevel2;
    treeLevel2.Create(4);

    // Combine all nodes into a global container
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starHub);
    allNodes.Add(starSpokes);
    allNodes.Add(treeRoot);
    allNodes.Add(treeLevel1);
    allNodes.Add(treeLevel2);

    // Point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install Internet Stack with OSPF routing
    InternetStackHelper stack;
    stack.SetRoutingHelper(OlsrHelper()); // OSPF not directly available; using OLSR as dynamic routing example
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    // Ring topology
    NetDeviceContainer ringDevices[4];
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t j = (i + 1) % 4;
        address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
        ringDevices[i] = p2p.Install(NodeContainer(ringNodes.Get(i), ringNodes.Get(j)));
        interfaces.push_back(address.Assign(ringDevices[i]));
    }

    // Mesh topology - fully connected
    NetDeviceContainer meshDevices[4][4];
    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = i + 1; j < 4; ++j) {
            address.SetBase("10.2." + std::to_string(i) + "." + std::to_string(j) + ".0", "255.255.255.0");
            meshDevices[i][j] = p2p.Install(NodeContainer(meshNodes.Get(i), meshNodes.Get(j)));
            interfaces.push_back(address.Assign(meshDevices[i][j]));
        }
    }

    // Bus topology (backbone linear connection)
    NetDeviceContainer busBackbone[4];
    for (uint32_t i = 0; i < 4; ++i) {
        address.SetBase("10.3." + std::to_string(i) + ".0", "255.255.255.0");
        busBackbone[i] = p2p.Install(NodeContainer(busNodes.Get(i), busNodes.Get(i+1)));
        interfaces.push_back(address.Assign(busBackbone[i]));
    }

    // Line topology
    NetDeviceContainer lineDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        address.SetBase("10.4." + std::to_string(i) + ".0", "255.255.255.0");
        lineDevices[i] = p2p.Install(NodeContainer(lineNodes.Get(i), lineNodes.Get(i+1)));
        interfaces.push_back(address.Assign(lineDevices[i]));
    }

    // Star topology
    NetDeviceContainer starLinks[5];
    for (uint32_t i = 0; i < 5; ++i) {
        address.SetBase("10.5." + std::to_string(i) + ".0", "255.255.255.0");
        starLinks[i] = p2p.Install(NodeContainer(starHub.Get(0), starSpokes.Get(i)));
        interfaces.push_back(address.Assign(starLinks[i]));
    }

    // Tree topology
    NetDeviceContainer treeLinks[6];
    uint32_t treeIdx = 0;

    // Root to level 1
    for (uint32_t i = 0; i < 2; ++i) {
        address.SetBase("10.6." + std::to_string(treeIdx) + ".0", "255.255.255.0");
        treeLinks[treeIdx++] = p2p.Install(NodeContainer(treeRoot.Get(0), treeLevel1.Get(i)));
        interfaces.push_back(address.Assign(treeLinks[treeIdx-1]));
    }

    // Level 1 to level 2
    for (uint32_t i = 0; i < 2; ++i) {
        for (uint32_t j = 0; j < 2; ++j) {
            address.SetBase("10.6." + std::to_string(treeIdx) + ".0", "255.255.255.0");
            treeLinks[treeIdx++] = p2p.Install(NodeContainer(treeLevel1.Get(i), treeLevel2.Get(2*i + j)));
            interfaces.push_back(address.Assign(treeLinks[treeIdx-1]));
        }
    }

    // Optional: Set positions for visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(20),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Enable NetAnim visualization
    AnimationInterface anim("hybrid-topology.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routes.xml", Seconds(0), Seconds(20), Seconds(0.25));

    // Setup applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(allNodes.Get(0)); // On first node
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(allNodes.Get(allNodes.GetN() - 1)); // Last node
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("hybrid-topology.tr");
    p2p.EnableAsciiAll(stream);
    p2p.EnablePcapAll("hybrid-topology");

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}