#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/animation-interface.h"
#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Read adjacency matrix from file
    std::ifstream adjFile("adjacency_matrix.txt");
    std::ifstream coordFile("node_coordinates.txt");

    uint32_t n;
    adjFile >> n;

    std::vector<std::vector<uint32_t>> adjMatrix(n, std::vector<uint32_t>(n));
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            adjFile >> adjMatrix[i][j];
        }
    }

    // Read node coordinates
    std::vector<std::pair<double, double>> coordinates(n);
    for (uint32_t i = 0; i < n; ++i) {
        coordFile >> coordinates[i].first >> coordinates[i].second;
    }

    adjFile.close();
    coordFile.close();

    // Create nodes
    NodeContainer nodes;
    nodes.Create(n);

    // Setup point-to-point links based on adjacency matrix
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    std::vector<NetDeviceContainer> deviceList;

    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = i + 1; j < n; ++j) {
            if (adjMatrix[i][j] == 1) {
                NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
                devices = p2p.Install(pair);
                deviceList.push_back(devices);
            }
        }
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    for (size_t i = 0; i < deviceList.size(); ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(address.Assign(deviceList[i]));
    }

    // Set up CBR traffic flows
    uint16_t port = 9;
    ApplicationContainer apps;

    for (uint32_t src = 0; src < n; ++src) {
        for (uint32_t dst = 0; dst < n; ++dst) {
            if (src != dst) {
                // Find route to destination
                Ptr<Node> sourceNode = nodes.Get(src);
                Ptr<Ipv4> ipv4 = sourceNode->GetObject<Ipv4>();
                Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1, 0); // Assume first interface is relevant
                Ipv4Address localIp = iaddr.GetLocal();

                OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                onoff.SetConstantRate(DataRate("1Mbps"), 512);
                onoff.SetAttribute("Local", AddressValue(Address(localIp)));

                apps.Add(onoff.Install(nodes.Get(src)));

                PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                apps.Add(sink.Install(nodes.Get(dst)));
                port++;
            }
        }
    }

    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Mobility model for visualization
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (const auto &coord : coordinates) {
        positionAlloc->Add(Vector(coord.first, coord.second, 0));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);

    // Animation output
    AnimationInterface anim("network_topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0); // Ensure positions are set even if mobility not used

    // Enable logging
    LogComponentEnable("NetworkTopologySimulation", LOG_LEVEL_INFO);
    NS_LOG_INFO("Starting simulation.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}