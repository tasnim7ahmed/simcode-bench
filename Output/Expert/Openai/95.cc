#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

using namespace ns3;

std::vector<std::vector<int>> ReadAdjacencyMatrix(const std::string &filename, uint32_t &n)
{
    std::ifstream infile(filename);
    if (!infile.is_open())
        throw std::runtime_error("Failed to open adjacency matrix file.");

    std::vector<std::vector<int>> upperTriangular;
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        int val;
        std::vector<int> row;
        while (iss >> val)
            row.push_back(val);
        upperTriangular.push_back(row);
    }
    infile.close();
    n = upperTriangular.size();
    // Fill lower triangle to make full adjacency matrix
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
    for (uint32_t i = 0; i < n; ++i)
    {
        for (uint32_t j = 0; j < n; ++j)
        {
            if (j >= i)
                matrix[i][j] = upperTriangular[i][j - i];
            else
                matrix[i][j] = matrix[j][i];
        }
    }
    return matrix;
}

std::vector<std::pair<double, double>> ReadNodeCoordinates(const std::string &filename, uint32_t n)
{
    std::ifstream infile(filename);
    if (!infile.is_open())
        throw std::runtime_error("Failed to open node coordinates file.");

    std::vector<std::pair<double, double>> coords;
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        double x, y;
        if (!(iss >> x >> y))
            throw std::runtime_error("Malformed coordinates file.");
        coords.emplace_back(x, y);
    }
    infile.close();

    if (coords.size() != n)
        throw std::runtime_error("Mismatch between adjacency matrix and coordinates count.");
    return coords;
}

int main(int argc, char *argv[])
{
    std::string adjMatrixFile = "adjacency_matrix.txt";
    std::string coordsFile = "node_coordinates.txt";
    std::string animFile = "netanim.xml";
    uint32_t n;

    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    auto adjMatrix = ReadAdjacencyMatrix(adjMatrixFile, n);
    auto nodeCoords = ReadNodeCoordinates(coordsFile, n);

    NodeContainer nodes;
    nodes.Create(n);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Maps a pair (i,j) -> NetDevice pairs for each point-to-point link
    std::vector<NetDeviceContainer> p2pDevices;
    std::vector<std::pair<uint32_t, uint32_t>> linkPairs;
    // For Ipv4 assignment: each link gets a /30 subnet
    Ipv4AddressHelper address;
    uint32_t subnetCnt = 1;
    std::map<std::pair<uint32_t, uint32_t>, std::pair<Ipv4InterfaceContainer, Ipv4InterfaceContainer>> linkIpInterfaces;

    // Build links based on adjacency matrix
    for (uint32_t i = 0; i < n; ++i)
    {
        for (uint32_t j = i + 1; j < n; ++j)
        {
            if (adjMatrix[i][j])
            {
                NodeContainer pair;
                pair.Add(nodes.Get(i));
                pair.Add(nodes.Get(j));
                NetDeviceContainer devices = p2p.Install(pair);
                p2pDevices.push_back(devices);
                linkPairs.emplace_back(i, j);

                std::ostringstream subnet;
                subnet << "10." << subnetCnt << ".0.0";
                address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.252");
                Ipv4InterfaceContainer interfaces = address.Assign(devices);
                // Store both (i,j) and (j,i)
                linkIpInterfaces[{i, j}] = {Ipv4InterfaceContainer(interfaces.Get(0)), Ipv4InterfaceContainer(interfaces.Get(1))};
                linkIpInterfaces[{j, i}] = {Ipv4InterfaceContainer(interfaces.Get(1)), Ipv4InterfaceContainer(interfaces.Get(0))};
                subnetCnt++;
            }
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    double cbRateMbps = 1.0;
    std::string cbPacketSize = "512";
    double startTime = 1.0;
    double stopTime = 10.0;
    ApplicationContainer apps;

    for (uint32_t src = 0; src < n; ++src)
    {
        for (uint32_t dst = 0; dst < n; ++dst)
        {
            if (src == dst)
                continue;
            // Get destination node's main IP address
            Ptr<Node> dstNode = nodes.Get(dst);
            Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
            Ipv4Address dstAddr = ipv4->GetAddress(1, 0).GetLocal(); // interface 1: first PPP link assigned
            uint16_t port = 5000 + src * n + dst;

            // Install packet sink on destination
            Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
            PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
            apps.Add(sinkHelper.Install(nodes.Get(dst)));

            // Install OnOff CBR app on source
            OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(dstAddr, port));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(static_cast<uint32_t>(cbRateMbps * 1'000'000)) + "bps")));
            onoff.SetAttribute("PacketSize", UintegerValue(std::stoi(cbPacketSize)));
            onoff.SetAttribute("StartTime", TimeValue(Seconds(startTime + ((src * n + dst) * 0.01))));
            onoff.SetAttribute("StopTime", TimeValue(Seconds(stopTime)));
            apps.Add(onoff.Install(nodes.Get(src)));
        }
    }

    AnimationInterface anim(animFile);
    anim.SetMaxPktsPerTraceFile(500000);
    // Set node positions
    for (uint32_t i = 0; i < n; ++i)
    {
        anim.SetConstantPosition(nodes.Get(i), nodeCoords[i].first, nodeCoords[i].second);
    }

    Simulator::Stop(Seconds(stopTime+1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}