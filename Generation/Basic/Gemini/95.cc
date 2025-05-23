#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdjacencyMatrixP2P");

// Global variables to store data read from files
static int g_numNodes = 0;
static std::vector<std::vector<int>> g_adjacencyMatrix;
static std::vector<Vector> g_nodeCoordinates;

/**
 * @brief Reads the adjacency matrix from the specified file.
 *
 * The file format is expected to be:
 * N (number of nodes)
 * matrix[0][0] matrix[0][1] ... matrix[0][N-1]
 * ...
 * matrix[N-1][0] ... matrix[N-1][N-1]
 *
 * @param filename The name of the file containing the adjacency matrix.
 * @return True if successful, false otherwise.
 */
bool ReadAdjacencyMatrix(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open())
    {
        NS_LOG_ERROR("Could not open adjacency matrix file: " << filename);
        return false;
    }

    ifs >> g_numNodes;
    if (ifs.fail() || g_numNodes <= 0)
    {
        NS_LOG_ERROR("Invalid or missing number of nodes in adjacency matrix file: " << filename);
        return false;
    }

    g_adjacencyMatrix.resize(g_numNodes, std::vector<int>(g_numNodes));

    for (int i = 0; i < g_numNodes; ++i)
    {
        for (int j = 0; j < g_numNodes; ++j)
        {
            if (!(ifs >> g_adjacencyMatrix[i][j]))
            {
                NS_LOG_ERROR("Failed to read matrix element at (" << i << "," << j << ") from " << filename);
                return false;
            }
        }
    }
    ifs.close();
    NS_LOG_INFO("Successfully read " << g_numNodes << " nodes and adjacency matrix from " << filename);
    return true;
}

/**
 * @brief Reads node coordinates from the specified file.
 *
 * Each line in the file should contain space-separated x, y, and optionally z coordinates.
 * e.g., "10.0 20.0 0.0" or "10.0 20.0"
 *
 * @param filename The name of the file containing the node coordinates.
 * @return True if successful, false otherwise.
 */
bool ReadNodeCoordinates(const std::string& filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open())
    {
        NS_LOG_ERROR("Could not open node coordinates file: " << filename);
        return false;
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty()) continue; // Skip empty lines
        std::istringstream iss(line);
        double x, y, z = 0.0; // Default z to 0.0 if not specified

        if (!(iss >> x >> y))
        {
            NS_LOG_ERROR("Invalid coordinate format in file: " << filename << " line: " << line);
            return false;
        }
        // Attempt to read z, if it fails, z remains 0.0
        iss >> z;
        
        g_nodeCoordinates.push_back(Vector(x, y, z));
    }
    ifs.close();

    if (g_nodeCoordinates.size() != static_cast<size_t>(g_numNodes))
    {
        NS_LOG_ERROR("Mismatch in node counts. Adjacency matrix defines " << g_numNodes
                    << " nodes, but " << g_nodeCoordinates.size() << " coordinates were read from " << filename);
        return false;
    }
    NS_LOG_INFO("Successfully read " << g_nodeCoordinates.size() << " node coordinates from " << filename);
    return true;
}

int main(int argc, char *argv[])
{
    // Enable logging for relevant components
    LogComponentEnable("AdjacencyMatrixP2P", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_INFO);


    // Default file names and simulation parameters
    std::string adjacencyFile = "adjacency_matrix.txt";
    std::string coordinatesFile = "node_coordinates.txt";
    double simulationTime = 10.0; // seconds
    std::string animFile = "netanim_output.xml";
    std::string traceFilePrefix = "p2p_cbr_trace"; // .pcap extension will be added by ns-3

    // Command line arguments parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("adjacencyFile", "Path to the adjacency matrix file", adjacencyFile);
    cmd.AddValue("coordinatesFile", "Path to the node coordinates file", coordinatesFile);
    cmd.AddValue("simTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("animFile", "NetAnim animation output file name", animFile);
    cmd.AddValue("traceFilePrefix", "Prefix for Pcap trace output files", traceFilePrefix);
    cmd.Parse(argc, argv);

    // Read input files
    if (!ReadAdjacencyMatrix(adjacencyFile))
    {
        return 1; // Exit if adjacency matrix file reading fails
    }
    if (!ReadNodeCoordinates(coordinatesFile))
    {
        return 1; // Exit if node coordinates file reading fails
    }

    if (g_numNodes == 0)
    {
        NS_LOG_ERROR("No nodes specified in the adjacency matrix. Exiting simulation.");
        return 1;
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(g_numNodes);

    // Install internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Configure Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.252"); // Use /30 subnet for each link

    int linkCount = 0;
    for (int i = 0; i < g_numNodes; ++i)
    {
        for (int j = i + 1; j < g_numNodes; ++j) // Iterate only upper triangular for unique links
        {
            if (g_adjacencyMatrix[i][j] == 1)
            {
                NS_LOG_INFO("Creating P2P link between Node " << i << " and Node " << j);
                NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(j));
                
                // Assign IP addresses to the devices on this link
                ipv4.Assign(devices);
                
                // Increment IP address for the next link to ensure unique subnets
                ipv4.NewNetwork();
                linkCount++;
            }
        }
    }
    NS_LOG_INFO("Successfully created " << linkCount << " Point-to-Point links.");

    // Populate global routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup CBR traffic flows: Each node sends to every other node (n*(n-1) flows)
    uint16_t currentSinkPort = 9; // Starting port for sinks
    
    for (int i = 0; i < g_numNodes; ++i) // Source node
    {
        for (int j = 0; j < g_numNodes; ++j) // Destination node
        {
            if (i == j)
            {
                continue; // A node does not send traffic to itself
            }

            // Get an IP address for the destination node (Node j) that is routable.
            // We assume the first non-loopback interface (index 1) IP is suitable.
            Ptr<Ipv4> ipv4OfDest = nodes.Get(j)->GetObject<Ipv4>();
            Ipv4Address destAddress = ipv4OfDest->GetAddress(1,0).GetLocal(); // Get IP of first non-loopback interface

            // Create PacketSink application on the destination node (Node j)
            PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), currentSinkPort));
            ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(j));
            sinkApps.Start(Seconds(0.0));
            sinkApps.Stop(Seconds(simulationTime));
            NS_LOG_INFO("PacketSink installed on Node " << j << " at port " << currentSinkPort);

            // Create OnOffApplication on the source node (Node i)
            OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(destAddress, currentSinkPort));
            onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always on
            onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Always off
            onoffHelper.SetAttribute("DataRate", StringValue("500kbps")); // Example data rate
            onoffHelper.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet

            ApplicationContainer onoffApps = onoffHelper.Install(nodes.Get(i));
            onoffApps.Start(Seconds(1.0)); // Start traffic after 1 second to allow routing to settle
            onoffApps.Stop(Seconds(simulationTime - 0.5)); // Stop traffic slightly before simulation ends
            NS_LOG_INFO("OnOffApplication installed on Node " << i << " sending to Node " << j
                        << " (Dest IP: " << destAddress << ", Port: " << currentSinkPort << ")");

            currentSinkPort++; // Increment port for the next unique flow
        }
    }
    NS_LOG_INFO("Configured " << (currentSinkPort - 9) << " CBR traffic flows.");

    // Setup NetAnim visualization
    AnimationInterface anim(animFile);
    for (int i = 0; i < g_numNodes; ++i)
    {
        anim.SetNodeDescription(nodes.Get(i), "Node " + std::to_string(i));
        // NetAnim's SetConstantPosition takes x, y. Z coordinate is typically ignored for 2D visualization.
        anim.SetConstantPosition(nodes.Get(i), g_nodeCoordinates[i].x, g_nodeCoordinates[i].y);
    }
    NS_LOG_INFO("NetAnim visualization enabled. Output to " << animFile);

    // Enable Pcap tracing for all Point-to-Point devices
    p2p.EnablePcapAll(traceFilePrefix, true); // true for promiscuous mode
    NS_LOG_INFO("Pcap tracing enabled. Output files prefixed with " << traceFilePrefix);

    // Set simulation stop time
    Simulator::Stop(Seconds(simulationTime));

    // Run the simulation
    NS_LOG_INFO("Starting simulation for " << simulationTime << " seconds...");
    Simulator::Run();
    NS_LOG_INFO("Simulation finished.");

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}