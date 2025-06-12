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

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("NetAnimHelper", LOG_LEVEL_INFO);

    // Default simulation time in seconds
    double simTime = 10.0;
    std::string matrixFile = "adjacency_matrix.txt";
    std::string coordFile = "node_coordinates.txt";
    std::string animFile = "output.xml";

    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation Time in Seconds", simTime);
    cmd.AddValue("matrixFile", "Adjacency Matrix Input File", matrixFile);
    cmd.AddValue("coordFile", "Node Coordinates Input File", coordFile);
    cmd.AddValue("animFile", "Output Animation XML File", animFile);
    cmd.Parse(argc, argv);

    // Read adjacency matrix
    std::ifstream matIn(matrixFile);
    uint32_t n;
    matIn >> n;

    std::vector<std::vector<int>> adjMatrix(n, std::vector<int>(n));
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            matIn >> adjMatrix[i][j];
        }
    }

    // Read node coordinates
    std::ifstream coordIn(coordFile);
    std::vector<Vector> positions(n);
    for (uint32_t i = 0; i < n; ++i) {
        double x, y;
        coordIn >> x >> y;
        positions[i] = Vector(x, y, 0.0);
    }

    NodeContainer nodes;
    nodes.Create(n);

    // Setup PointToPoint links based on adjacency matrix
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devices(n);

    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (adjMatrix[i][j] == 1) {
                NetDeviceContainer dev = p2p.Install(nodes.Get(i), nodes.Get(j));
            }
        }
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    std::vector<Ipv4InterfaceContainer> interfaces(n);

    // Assign IP addresses to all devices
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (adjMatrix[i][j] == 1) {
                address.NewNetwork();
                interfaces[i] = address.Assign(devices[j]); // Simplified assignment - actual logic may vary
            }
        }
    }

    // Set up CBR traffic between all pairs
    uint16_t port = 9;
    for (uint32_t src = 0; src < n; ++src) {
        for (uint32_t dst = 0; dst < n; ++dst) {
            if (src != dst) {
                Address sinkAddress(InetSocketAddress(positions[dst].x, port));
                PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(dst));
                sinkApp.Start(Seconds(0.0));
                sinkApp.Stop(Seconds(simTime));

                OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
                onoff.SetConstantRate(DataRate("1Mbps"), 512);
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

                ApplicationContainer apps = onoff.Install(nodes.Get(src));
                apps.Start(Seconds(1.0));
                apps.Stop(Seconds(simTime));
            }
        }
    }

    // Mobility model for visualization
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

    // Set initial positions from file
    for (uint32_t i = 0; i < n; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<MobilityModel> mobilityModel = node->GetObject<MobilityModel>();
        mobilityModel->SetPosition(positions[i]);
    }

    // NetAnim Visualization
    AnimationInterface anim(animFile);
    for (uint32_t i = 0; i < n; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i)->GetId(), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i)->GetId(), 255, 0, 0); // Red color
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}