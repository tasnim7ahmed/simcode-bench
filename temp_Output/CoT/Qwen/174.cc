#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"
#include "ns3/ipv4-routing-table-entry.h"

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
    ringNodes.Create(5); // Ring topology with 5 nodes

    NodeContainer meshNodes;
    meshNodes.Create(4); // Mesh topology with 4 nodes (fully connected)

    NodeContainer busStations;
    busStations.Create(1); // Single node as bus backbone controller
    NodeContainer busNodes;
    busNodes.Create(4); // Bus clients connected to the single backbone station

    NodeContainer lineNodes;
    lineNodes.Create(5); // Line topology: linear chain of 5 nodes

    NodeContainer starHub;
    starHub.Create(1); // Center node of star
    NodeContainer starSpokes;
    starSpokes.Create(5); // Spoke nodes around the center

    NodeContainer treeRoot;
    treeRoot.Create(1); // Root of the tree
    NodeContainer treeLevel1;
    treeLevel1.Create(2); // Level 1 children
    NodeContainer treeLevel2;
    treeLevel2.Create(4); // Level 2 grandchildren

    // Combine all nodes into a global container
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busStations);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starHub);
    allNodes.Add(starSpokes);
    allNodes.Add(treeRoot);
    allNodes.Add(treeLevel1);
    allNodes.Add(treeLevel2);

    // PointToPointHelper setup
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install InternetStack on all nodes
    InternetStackHelper stack;
    stack.SetRoutingHelper(OlsrHelper()); // Enable dynamic routing using OLSR
    stack.Install(allNodes);

    // Hybrid topology connections
    NetDeviceContainer ringDevices;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i) {
        uint32_t j = (i + 1) % ringNodes.GetN();
        ringDevices.Add(p2p.Install(ringNodes.Get(i), ringNodes.Get(j)));
    }

    NetDeviceContainer meshDevices;
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        for (uint32_t k = i + 1; k < meshNodes.GetN(); ++k) {
            meshDevices.Add(p2p.Install(meshNodes.Get(i), meshNodes.Get(k)));
        }
    }

    NetDeviceContainer busDevices;
    for (uint32_t i = 0; i < busNodes.GetN(); ++i) {
        busDevices.Add(p2p.Install(busStations.Get(0), busNodes.Get(i)));
    }

    NetDeviceContainer lineDevices;
    for (uint32_t i = 0; i < lineNodes.GetN() - 1; ++i) {
        lineDevices.Add(p2p.Install(lineNodes.Get(i), lineNodes.Get(i+1)));
    }

    NetDeviceContainer starDevices;
    for (uint32_t i = 0; i < starSpokes.GetN(); ++i) {
        starDevices.Add(p2p.Install(starHub.Get(0), starSpokes.Get(i)));
    }

    NetDeviceContainer treeDevices;
    treeDevices.Add(p2p.Install(treeRoot.Get(0), treeLevel1.Get(0)));
    treeDevices.Add(p2p.Install(treeRoot.Get(0), treeLevel1.Get(1)));

    treeDevices.Add(p2p.Install(treeLevel1.Get(0), treeLevel2.Get(0)));
    treeDevices.Add(p2p.Install(treeLevel1.Get(0), treeLevel2.Get(1)));
    treeDevices.Add(p2p.Install(treeLevel1.Get(1), treeLevel2.Get(2)));
    treeDevices.Add(p2p.Install(treeLevel1.Get(1), treeLevel2.Get(3)));

    // Interconnecting all topology segments
    NetDeviceContainer interconnectDevices;
    interconnectDevices.Add(p2p.Install(ringNodes.Get(0), meshNodes.Get(0)));
    interconnectDevices.Add(p2p.Install(meshNodes.Get(1), busStations.Get(0)));
    interconnectDevices.Add(p2p.Install(busNodes.Get(0), lineNodes.Get(0)));
    interconnectDevices.Add(p2p.Install(lineNodes.Get(4), starHub.Get(0)));
    interconnectDevices.Add(p2p.Install(starSpokes.Get(0), treeRoot.Get(0)));

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(ringDevices);

    address.NewNetwork();
    address.Assign(meshDevices);

    address.NewNetwork();
    address.Assign(busDevices);

    address.NewNetwork();
    address.Assign(lineDevices);

    address.NewNetwork();
    address.Assign(starDevices);

    address.NewNetwork();
    address.Assign(treeDevices);

    address.NewNetwork();
    address.Assign(interconnectDevices);

    // Set up applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(allNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(allNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < allNodes.GetN(); ++i) {
        clientApps = echoClient.Install(allNodes.Get(i));
        clientApps.Start(Seconds(2.0 + i));
        clientApps.Stop(Seconds(10.0));
    }

    // Mobility model for visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(20),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // NetAnim Visualization
    AnimationInterface anim("hybrid-topology.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(5), Seconds(0.25));

    // PCAP tracing
    p2p.EnablePcapAll("hybrid_topology");

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}