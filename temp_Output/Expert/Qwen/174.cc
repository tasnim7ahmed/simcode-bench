#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Enable OspfRouting for dynamic routing
    Config::SetDefault("ns3::Ipv4GlobalRouting::PerflowEcmp", BooleanValue(true));
    Config::SetDefault("ns3::OspfV2RoutingProtocol::UseHysteresis", BooleanValue(false));

    NodeContainer ringNodes;
    ringNodes.Create(5);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(3);

    NodeContainer lineNodes;
    lineNodes.Create(4);

    NodeContainer starHub;
    starHub.Create(1);
    NodeContainer starSpokes;
    starSpokes.Create(5);

    NodeContainer treeRoot;
    treeRoot.Create(1);
    NodeContainer treeLevel1;
    treeLevel1.Create(2);
    NodeContainer treeLevel2;
    treeLevel2.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.SetRoutingHelper(OspfHelper()); // Dynamic routing with OSPF
    stack.InstallAll();

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    // Ring topology
    NodeContainer ringPair;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i) {
        ringPair = NodeContainer(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        NetDeviceContainer devices = p2p.Install(ringPair);
        address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }

    // Mesh topology
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        for (uint32_t j = i + 1; j < meshNodes.GetN(); ++j) {
            NetDeviceContainer devices = p2p.Install(NodeContainer(meshNodes.Get(i), meshNodes.Get(j)));
            address.SetBase("10.2." + std::to_string(i * 10 + j) + ".0", "255.255.255.0");
            interfaces.push_back(address.Assign(devices));
        }
    }

    // Bus topology
    for (uint32_t i = 0; i < busNodes.GetN() - 1; ++i) {
        NetDeviceContainer devices = p2p.Install(NodeContainer(busNodes.Get(i), busNodes.Get(i + 1)));
        address.SetBase("10.3." + std::to_string(i) + ".0", "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }

    // Line topology
    for (uint32_t i = 0; i < lineNodes.GetN() - 1; ++i) {
        NetDeviceContainer devices = p2p.Install(NodeContainer(lineNodes.Get(i), lineNodes.Get(i + 1)));
        address.SetBase("10.4." + std::to_string(i) + ".0", "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }

    // Star topology
    for (uint32_t i = 0; i < starSpokes.GetN(); ++i) {
        NetDeviceContainer devices = p2p.Install(NodeContainer(starHub.Get(0), starSpokes.Get(i)));
        address.SetBase("10.5." + std::to_string(i) + ".0", "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }

    // Tree topology
    // Root to level 1
    for (uint32_t i = 0; i < treeLevel1.GetN(); ++i) {
        NetDeviceContainer devices = p2p.Install(NodeContainer(treeRoot.Get(0), treeLevel1.Get(i)));
        address.SetBase("10.6." + std::to_string(i) + ".0", "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }
    // Level 1 to level 2
    for (uint32_t i = 0; i < treeLevel1.GetN(); ++i) {
        for (uint32_t j = 0; j < 2; ++j) {
            uint32_t nodeIndex = i * 2 + j;
            if (nodeIndex >= treeLevel2.GetN()) break;
            NetDeviceContainer devices = p2p.Install(NodeContainer(treeLevel1.Get(i), treeLevel2.Get(nodeIndex)));
            address.SetBase("10.7." + std::to_string(nodeIndex) + ".0", "255.255.255.0");
            interfaces.push_back(address.Assign(devices));
        }
    }

    // Connect topologies together (hybrid interconnection)
    NetDeviceContainer hybridLink1 = p2p.Install(NodeContainer(ringNodes.Get(0), meshNodes.Get(0)));
    address.SetBase("10.8.1.0", "255.255.255.0");
    interfaces.push_back(address.Assign(hybridLink1));

    NetDeviceContainer hybridLink2 = p2p.Install(NodeContainer(meshNodes.Get(3), busNodes.Get(0)));
    address.SetBase("10.8.2.0", "255.255.255.0");
    interfaces.push_back(address.Assign(hybridLink2));

    NetDeviceContainer hybridLink3 = p2p.Install(NodeContainer(busNodes.Get(2), lineNodes.Get(0)));
    address.SetBase("10.8.3.0", "255.255.255.0");
    interfaces.push_back(address.Assign(hybridLink3));

    NetDeviceContainer hybridLink4 = p2p.Install(NodeContainer(lineNodes.Get(3), starHub.Get(0)));
    address.SetBase("10.8.4.0", "255.255.255.0");
    interfaces.push_back(address.Assign(hybridLink4));

    NetDeviceContainer hybridLink5 = p2p.Install(NodeContainer(starSpokes.Get(4), treeRoot.Get(0)));
    address.SetBase("10.8.5.0", "255.255.255.0");
    interfaces.push_back(address.Assign(hybridLink5));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Tracing setup
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("hybrid-topology.tr"));

    // Mobility for visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(20),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    // NetAnim visualization
    AnimationInterface anim("hybrid-topology.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(20), Seconds(0.25));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}