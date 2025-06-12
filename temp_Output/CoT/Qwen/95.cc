#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t n = 0;
    std::string matrixFile = "adjacency_matrix.txt";
    std::string coordFile = "node_coordinates.txt";
    double simDuration = 10.0;
    double cbrInterval = 0.005; // seconds
    uint32_t cbrPacketSize = 512; // bytes

    CommandLine cmd;
    cmd.AddValue("matrix", "Adjacency matrix file name", matrixFile);
    cmd.AddValue("coords", "Node coordinates file name", coordFile);
    cmd.AddValue("duration", "Simulation duration in seconds", simDuration);
    cmd.Parse(argc, argv);

    // Read adjacency matrix
    std::ifstream matIn(matrixFile);
    if (!matIn.is_open()) {
        NS_FATAL_ERROR("Could not open adjacency matrix file: " << matrixFile);
    }
    matIn >> n;
    std::vector<std::vector<uint8_t>> adjMatrix(n, std::vector<uint8_t>(n));
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i; j < n; ++j) {
            matIn >> adjMatrix[i][j];
            adjMatrix[j][i] = adjMatrix[i][j]; // Mirror to make it symmetric
        }
    }
    matIn.close();

    // Read node coordinates
    std::ifstream coordIn(coordFile);
    if (!coordIn.is_open()) {
        NS_FATAL_ERROR("Could not open node coordinates file: " << coordFile);
    }
    std::vector<Vector> coords(n);
    for (uint32_t i = 0; i < n; ++i) {
        double x, y;
        coordIn >> x >> y;
        coords[i] = Vector(x, y, 0.0);
    }
    coordIn.close();

    NodeContainer nodes;
    nodes.Create(n);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    std::vector<NetDeviceContainer> devices(n);
    std::vector<Ipv4InterfaceContainer> interfaces(n);

    // Create a grid of point-to-point links based on adjacency matrix
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (adjMatrix[i][j]) {
                NetDeviceContainer nd = p2p.Install(NodeContainer(nodes.Get(i), nodes.Get(j)));
                address.SetBase("10." + std::to_string(i) + "." + std::to_string(j) + ".0", "255.255.255.0");
                Ipv4InterfaceContainer ipf = address.Assign(nd);
                devices[i].Add(nd.Get(0));
                devices[j].Add(nd.Get(1));
                interfaces[i].Add(ipf.Get(0));
                interfaces[j].Add(ipf.Get(1));
            }
        }
    }

    // Assign IP addresses and create routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install CBR traffic sources and sinks
    uint16_t port = 9;
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            if (i != j) {
                Address sinkAddress(InetSocketAddress(interfaces[j].GetAddress(0), port));
                PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(j));
                sinkApps.Start(Seconds(0.0));
                sinkApps.Stop(Seconds(simDuration));

                OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
                onoff.SetConstantRate(DataRate((uint64_t)(cbrPacketSize * 8 / cbrInterval)));
                onoff.SetAttribute("PacketSize", UintegerValue(cbrPacketSize));
                ApplicationContainer srcApps = onoff.Install(nodes.Get(i));
                srcApps.Start(Seconds(1.0));
                srcApps.Stop(Seconds(simDuration - 1.0));
            }
        }
    }

    // Setup mobility for visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(n),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Manually set positions from the coordinate file
    for (uint32_t i = 0; i < n; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        mob->SetPosition(coords[i]);
    }

    // Enable NetAnim output
    AnimationInterface anim("network_simulation.xml");
    anim.SetMaxPktsPerTraceFile(5000000);
    for (uint32_t i = 0; i < n; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}