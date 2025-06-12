#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include "ns3/mobility-module.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

using namespace ns3;

std::vector<std::vector<int>> ReadAdjacencyMatrix(const std::string& filename, uint32_t& n)
{
    std::vector<std::vector<int>> matrix;
    std::ifstream fin(filename);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(fin, line)) lines.push_back(line);
    n = lines.size();
    matrix.resize(n, std::vector<int>(n, 0));
    for (uint32_t i = 0; i < n; ++i)
    {
        std::istringstream iss(lines[i]);
        for (uint32_t j = i; j < n; ++j)
        {
            int val;
            iss >> val;
            matrix[i][j] = val;
            matrix[j][i] = val; // Symmetric for undirected
        }
    }
    return matrix;
}

std::vector<std::pair<double, double>> ReadNodeCoordinates(const std::string& filename, uint32_t n)
{
    std::vector<std::pair<double, double>> coords(n);
    std::ifstream fin(filename);
    std::string line;
    uint32_t idx = 0;
    while (std::getline(fin, line) && idx < n)
    {
        std::istringstream iss(line);
        double x, y;
        iss >> x >> y;
        coords[idx] = std::make_pair(x, y);
        ++idx;
    }
    return coords;
}

int
main(int argc, char *argv[])
{
    std::string adjacencyFile = "adjacency_matrix.txt";
    std::string coordFile = "node_coordinates.txt";
    std::string animFile = "wired-netanim.xml";

    CommandLine cmd;
    cmd.AddValue("matrix", "Adjacency matrix file", adjacencyFile);
    cmd.AddValue("coords", "Coordinates file", coordFile);
    cmd.AddValue("anim", "NetAnim output file", animFile);
    cmd.Parse(argc, argv);

    uint32_t nNodes = 0;
    auto adjacencyMatrix = ReadAdjacencyMatrix(adjacencyFile, nNodes);
    auto nodeCoords = ReadNodeCoordinates(coordFile, nNodes);

    NodeContainer nodes;
    nodes.Create(nNodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    // Place nodes using MobilityHelper (for NetAnim visualization)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (const auto& coord : nodeCoords)
        positionAlloc->Add(Vector(coord.first, coord.second, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Create P2P links as per adjacency matrix, record interfaces for assignment
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    std::vector<std::pair<uint32_t, uint32_t>> links;

    std::vector<NetDeviceContainer> devsList;
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        for (uint32_t j = i + 1; j < nNodes; ++j)
        {
            if (adjacencyMatrix[i][j])
            {
                NodeContainer nc;
                nc.Add(nodes.Get(i));
                nc.Add(nodes.Get(j));
                NetDeviceContainer ndc = p2p.Install(nc);
                devsList.push_back(ndc);
                links.emplace_back(i, j);
            }
        }
    }

    // Assign IP addresses to each point-to-point link
    std::vector<Ipv4InterfaceContainer> ifacesList;
    Ipv4AddressHelper address;
    uint32_t subnet = 1;
    for (auto& ndc : devsList)
    {
        std::ostringstream subnetAddr;
        subnetAddr << "10.1." << subnet << ".0";
        address.SetBase(Ipv4Address(subnetAddr.str().c_str()), "255.255.255.0");
        ifacesList.push_back(address.Assign(ndc));
        subnet++;
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // For each node, create a CBR flow (OnOffApplication) to every other node
    uint16_t port = 9000;
    double appStart = 1.0;
    double appStop = 19.0;
    double simStop = 20.0;
    std::vector<ApplicationContainer> appContainers;

    for (uint32_t src = 0; src < nNodes; ++src)
    {
        for (uint32_t dst = 0; dst < nNodes; ++dst)
        {
            if (src == dst) continue;
            // Get Ipv4Address of dst node (use first interface)
            Ptr<Node> dstNode = nodes.Get(dst);
            Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
            Ipv4Address dstAddr = ipv4->GetAddress(1,0).GetLocal();

            OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(dstAddr, port)));
            onoff.SetConstantRate(DataRate("1Mbps"), 1024); // 1 Mbps, 1024 byte packets
            onoff.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
            onoff.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
            ApplicationContainer app = onoff.Install(nodes.Get(src));
            appContainers.push_back(app);

            // PacketSink for receiver
            PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
            ApplicationContainer sinkApp = sink.Install(nodes.Get(dst));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(simStop));
        }
    }

    // Logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // NetAnim
    AnimationInterface anim(animFile);

    // Label nodes
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        anim.UpdateNodeDescription(i, "Node" + std::to_string(i));
        anim.UpdateNodeColor(i, 0, 255, 0); // green
    }

    Simulator::Stop(Seconds(simStop));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}