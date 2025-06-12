#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

// Helper function to trim whitespace
std::string Trim(const std::string& str)
{
    const char* whitespace = " \t\n\r";
    const size_t begin = str.find_first_not_of(whitespace);
    if (begin == std::string::npos)
        return "";
    const size_t end = str.find_last_not_of(whitespace);
    return str.substr(begin, end - begin + 1);
}

// Function to read node coordinates from a file
std::vector<std::pair<double, double>> ReadNodeCoordinates(const std::string& filename)
{
    std::vector<std::pair<double, double>> coords;
    std::ifstream infile(filename);
    if (!infile.is_open())
    {
        NS_FATAL_ERROR("Unable to open node coordinates file: " << filename);
    }
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        double x, y;
        if (!(iss >> x >> y))
        {
            NS_FATAL_ERROR("Invalid line in node coordinates file: " << line);
        }
        coords.emplace_back(x, y);
    }
    infile.close();
    return coords;
}

// Function to read an upper triangular adjacency matrix from a file
std::vector<std::vector<int>> ReadAdjacencyMatrix(const std::string& filename, uint32_t n)
{
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
    std::ifstream infile(filename);
    if (!infile.is_open())
    {
        NS_FATAL_ERROR("Unable to open adjacency matrix file: " << filename);
    }
    std::string line;
    uint32_t i = 0;
    while (std::getline(infile, line) && i < n)
    {
        std::istringstream iss(line);
        for (uint32_t j = i; j < n; ++j)
        {
            int val;
            if (!(iss >> val))
            {
                NS_FATAL_ERROR("Invalid line or insufficient entries in adjacency matrix file at row " << i);
            }
            matrix[i][j] = val;
            matrix[j][i] = val; // symmetric for undirected
        }
        ++i;
    }
    infile.close();
    if (i != n)
    {
        NS_FATAL_ERROR("Adjacency matrix file row count mismatch with number of nodes");
    }
    return matrix;
}

int main(int argc, char *argv[])
{
    // Command line arguments
    std::string adjMatrixFile = "adjacency_matrix.txt";
    std::string coordFile = "node_coordinates.txt";
    std::string animFile = "network-animation.xml";
    double simTime = 10.0; // seconds
    double linkDataRateMbps = 10.0;
    double linkDelayMs = 2.0;
    uint32_t cbrPacketSize = 512;
    std::string cbrDataRate = "1Mbps";

    CommandLine cmd;
    cmd.AddValue("adjacencyMatrix", "Adjacency matrix file name", adjMatrixFile);
    cmd.AddValue("nodeCoordinates", "Node coordinates file name", coordFile);
    cmd.AddValue("animFile", "NetAnim output XML file", animFile);
    cmd.Parse(argc, argv);

    // Read coordinates file and determine number of nodes
    std::vector<std::pair<double, double>> coordinates = ReadNodeCoordinates(coordFile);
    uint32_t nNodes = coordinates.size();

    // Read adjacency matrix
    std::vector<std::vector<int>> adjacency = ReadAdjacencyMatrix(adjMatrixFile, nNodes);

    // Logging for debugging
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Set positions for NetAnim
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (const auto &coord : coordinates) {
        posAlloc->Add(Vector(coord.first, coord.second, 0.0));
    }
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Create all necessary p2p channels/devices
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(static_cast<uint64_t>(linkDataRateMbps * 1000000))));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(linkDelayMs)));

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<std::pair<uint32_t, uint32_t>> nodePairs;

    // Assign a subnet for each link
    uint32_t subnetCount = 0;
    std::vector<std::tuple<uint32_t, uint32_t, NetDeviceContainer>> links;

    for (uint32_t i = 0; i < nNodes; ++i) {
        for (uint32_t j = i + 1; j < nNodes; ++j) {
            if (adjacency[i][j] != 0) {
                NetDeviceContainer dc = p2p.Install(nodes.Get(i), nodes.Get(j));
                links.push_back(std::make_tuple(i, j, dc));
            }
        }
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<std::vector<Ipv4InterfaceContainer>> interfaces(nNodes, std::vector<Ipv4InterfaceContainer>(nNodes));

    subnetCount = 0;
    for (auto &link : links) {
        std::ostringstream subnet;
        subnet << "10." << ((subnetCount/255)+1) << "." << (subnetCount%255) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(std::get<2>(link));
        uint32_t i = std::get<0>(link);
        uint32_t j = std::get<1>(link);
        interfaces[i][j] = iface;
        interfaces[j][i] = iface;
        subnetCount++;
    }

    // Assign default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup unique CBR traffic flows: every node to every other node (except itself)
    uint16_t basePort = 9000;
    ApplicationContainer sinkApps, clientApps;
    for (uint32_t dst = 0; dst < nNodes; ++dst) {
        // Create a UDP Sink on each node to receive from all other CBR flows
        uint16_t port = basePort + dst;
        Address sinkAddr(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sink("ns3::UdpSocketFactory", sinkAddr);
        ApplicationContainer app = sink.Install(nodes.Get(dst));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simTime));
        sinkApps.Add(app);
    }
    for (uint32_t src = 0; src < nNodes; ++src) {
        for (uint32_t dst = 0; dst < nNodes; ++dst) {
            if (src == dst) continue;
            // Find an IP on dst that src can reach; pick the first interface
            Ipv4Address dstAddr;
            // Get the first address that src and dst share a link (as per above interface assignments)
            bool found = false;
            for (uint32_t k = 0; k < nNodes && !found; ++k) {
                if (k != src && k != dst && !interfaces[src][k].GetN() == 0 && !interfaces[k][dst].GetN() == 0)
                    continue;
            }
            // If there is a direct link, use that
            if (interfaces[src][dst].GetN() > 0) {
                dstAddr = interfaces[src][dst].GetAddress(1); // dst is the second in the pair
                found = true;
            }
            // Otherwise, pick the first address of dst from a link (any should do)
            if (!found) {
                for (uint32_t j = 0; j < nNodes; ++j) {
                    if (interfaces[dst][j].GetN() > 0) {
                        dstAddr = interfaces[dst][j].GetAddress(0);
                        found = true;
                        break;
                    }
                }
            }
            uint16_t port = basePort + dst;
            // OnOffApplication for CBR traffic
            OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(dstAddr, port));
            onoff.SetAttribute("PacketSize", UintegerValue(cbrPacketSize));
            onoff.SetAttribute("DataRate", DataRateValue(DataRate(cbrDataRate)));
            onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0 + 0.001 * src)));
            onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime - 0.01)));
            ApplicationContainer app = onoff.Install(nodes.Get(src));
            clientApps.Add(app);
        }
    }

    // Enable NetAnim output
    AnimationInterface anim(animFile);

    // Set node descriptions for NetAnim
    for (uint32_t i = 0; i < nNodes; ++i) {
        std::ostringstream desc;
        desc << "Node-" << i;
        anim.UpdateNodeDescription(i, desc.str());
        anim.UpdateNodeColor(i, 255, 127, 0); // orange
    }
    // Set up link visualization
    for (auto &link : links) {
        uint32_t i = std::get<0>(link);
        uint32_t j = std::get<1>(link);
        anim.SetLinkDescription(nodes.Get(i)->GetId(), nodes.Get(j)->GetId(), "P2P");
        anim.UpdateLinkColor(nodes.Get(i)->GetId(), nodes.Get(j)->GetId(), 0, 0, 255, 0.5); // blue
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}