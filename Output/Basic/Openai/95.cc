#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace ns3;

std::vector<std::vector<bool>> ReadAdjacencyMatrix(const std::string &filename, uint32_t &numNodes)
{
    std::ifstream file(filename.c_str());
    std::string line;
    std::vector<std::vector<bool>> matrix;

    numNodes = 0;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        bool value;
        std::vector<bool> row;
        while (iss >> value) row.push_back(value);
        if (!row.empty())
        {
            matrix.push_back(row);
            ++numNodes;
        }
    }
    // Convert upper triangular to full adjacency matrix
    std::vector<std::vector<bool>> adj(numNodes, std::vector<bool>(numNodes, false));
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        for (uint32_t j = i; j < matrix[i].size(); ++j)
        {
            adj[i][j] = matrix[i][j];
            if (i != j)
            {
                adj[j][i] = matrix[i][j];
            }
        }
    }
    return adj;
}

std::vector<Vector> ReadNodeCoordinates(const std::string &filename, uint32_t numNodes)
{
    std::ifstream file(filename.c_str());
    std::string line;
    std::vector<Vector> positions;
    uint32_t count = 0;

    while (std::getline(file, line) && count < numNodes)
    {
        std::istringstream iss(line);
        double x, y, z = 0.0;
        iss >> x >> y;
        if (!(iss >> z))
            z = 0.0;
        positions.emplace_back(Vector(x, y, z));
        ++count;
    }
    return positions;
}

int main(int argc, char *argv[])
{
    std::string adjacencyFile = "adjacency_matrix.txt";
    std::string coordinatesFile = "node_coordinates.txt";
    std::string animFile = "wired-netanim.xml";
    uint32_t numNodes = 0;

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Read adjacency matrix & coordinates
    std::vector<std::vector<bool>> adjacency = ReadAdjacencyMatrix(adjacencyFile, numNodes);
    std::vector<Vector> nodeCoords = ReadNodeCoordinates(coordinatesFile, numNodes);

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Store device and interface between each pair for NetAnim
    std::vector<std::vector<NetDeviceContainer>> ndc(numNodes, std::vector<NetDeviceContainer>(numNodes));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Create all point-to-point links described by upper-triangular adjacency matrix
    std::vector<std::vector<Ipv4InterfaceContainer>> interfaces(numNodes, std::vector<Ipv4InterfaceContainer>(numNodes));
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper address;
    uint32_t netNum = 1;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        for (uint32_t j = i + 1; j < numNodes; ++j)
        {
            if (adjacency[i][j])
            {
                NodeContainer nc(nodes.Get(i), nodes.Get(j));
                NetDeviceContainer dev = p2p.Install(nc);
                ndc[i][j] = dev;
                ndc[j][i] = dev;
                std::ostringstream subnet;
                subnet << "10." << netNum << ".0.0";
                address.SetBase(subnet.str().c_str(), "255.255.255.0");
                Ipv4InterfaceContainer iface = address.Assign(dev);
                interfaces[i][j] = iface;
                interfaces[j][i] = iface;
                ++netNum;
            }
        }
    }

    // Assign positions using MobilityHelper for NetAnim
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nodeCoords.size(); ++i)
    {
        positionAlloc->Add(nodeCoords[i]);
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Assign IP addresses and build the routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Launch a UdpServerHelper on each node for traffic reception
    std::vector<uint16_t> cbrPorts(numNodes);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint16_t port = 9000 + i;
        cbrPorts[i] = port;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(i));
        serverApp.Start(Seconds(0.5));
        serverApp.Stop(Seconds(15.0));
        serverApps.Add(serverApp);
    }

    // Start a unique traffic flow (from every node to every other node)
    double appStart = 1.0;
    double appStop = 14.0;
    std::string dataRate = "1Mbps";
    uint32_t pktSize = 512;
    uint32_t pktIntervalMs = 50; // 20 pkt/sec

    ApplicationContainer clientApps;
    for (uint32_t src = 0; src < numNodes; ++src)
    {
        for (uint32_t dst = 0; dst < numNodes; ++dst)
        {
            if (src == dst) continue;

            Ptr<Node> dstNode = nodes.Get(dst);
            Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
            Ipv4Address dstAddr = ipv4->GetAddress(1, 0).GetLocal(); // assumes first interface is P2P
            uint16_t port = cbrPorts[dst];

            UdpClientHelper client(dstAddr, port);
            client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
            client.SetAttribute("Interval", TimeValue(MilliSeconds(pktIntervalMs)));
            client.SetAttribute("PacketSize", UintegerValue(pktSize));
            client.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
            client.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
            ApplicationContainer clientApp = client.Install(nodes.Get(src));
            clientApps.Add(clientApp);
        }
    }

    // Set up NetAnim
    AnimationInterface anim(animFile);
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        anim.SetConstantPosition(nodes.Get(i), nodeCoords[i].x, nodeCoords[i].y, nodeCoords[i].z);
    }
    // Annotate links in NetAnim (show all links)
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        for (uint32_t j = i+1; j < numNodes; ++j)
        {
            if (adjacency[i][j])
            {
                anim.UpdateLinkDescription(ndc[i][j].Get(0), "n" + std::to_string(i) + "<->n" + std::to_string(j), 0);
            }
        }
    }

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}