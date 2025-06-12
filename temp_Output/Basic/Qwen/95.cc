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

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Read adjacency matrix
    std::ifstream adjFile("adjacency_matrix.txt");
    uint32_t n;
    adjFile >> n;

    std::vector<std::vector<uint32_t>> adjMatrix(n, std::vector<uint32_t>(n));
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            adjFile >> adjMatrix[i][j];
        }
    }

    // Read node coordinates
    std::ifstream coordFile("node_coordinates.txt");
    std::vector<std::pair<double, double>> nodeCoords(n);
    for (uint32_t i = 0; i < n; ++i) {
        double x, y;
        coordFile >> x >> y;
        nodeCoords[i] = std::make_pair(x, y);
    }

    NodeContainer nodes;
    nodes.Create(n);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[n];

    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (adjMatrix[i][j] == 1) {
                NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
                devices[i].Add(p2p.Install(pair).Get(0));
                devices[j].Add(p2p.Install(pair).Get(1));
            }
        }
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[n];

    uint32_t interfaceIndex = 0;
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (adjMatrix[i][j] == 1) {
                std::ostringstream subnet;
                subnet << "10." << interfaceIndex++ << ".0.0";
                address.SetBase(subnet.str().c_str(), "255.255.255.0");
                interfaces[i].Add(address.Assign(devices[i]));
                interfaces[j].Add(address.Assign(devices[j]));
            }
        }
    }

    // Mobility model for visualization
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(100.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set positions explicitly from file
    for (uint32_t i = 0; i < n; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        Vector position(nodeCoords[i].first, nodeCoords[i].second, 0);
        mobility->SetPosition(position);
    }

    // Install applications: CBR traffic between all pairs of nodes
    uint16_t port = 9;
    ApplicationContainer apps;

    for (uint32_t src = 0; src < n; ++src) {
        for (uint32_t dst = 0; dst < n; ++dst) {
            if (src != dst) {
                OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces[dst].GetAddress(0), port));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
                onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
                onoff.SetAttribute("PacketSize", UintegerValue(1024));

                apps.Add(onoff.Install(nodes.Get(src)));
            }
        }
    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("network-simulation.tr"));
    p2p.EnablePcapAll("network-simulation");

    AnimationInterface anim("network-simulation.xml");
    anim.SetMobilityPollInterval(Seconds(1));

    NS_LOG_INFO("Starting simulation.");
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}