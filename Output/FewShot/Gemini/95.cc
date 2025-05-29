#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {
    std::string adjMatrixFile = "adjacency_matrix.txt";
    std::string nodeCoordsFile = "node_coordinates.txt";

    // Read adjacency matrix
    std::ifstream adjFile(adjMatrixFile);
    std::string line;
    std::vector<std::vector<int>> adjMatrix;
    while (std::getline(adjFile, line)) {
        std::stringstream ss(line);
        int value;
        char comma;
        std::vector<int> row;
        while (ss >> value) {
            row.push_back(value);
            ss >> comma; // Consume the comma, or ignore if it's the last element
        }
        adjMatrix.push_back(row);
    }
    adjFile.close();

    // Read node coordinates
    std::ifstream coordFile(nodeCoordsFile);
    std::vector<std::pair<double, double>> nodeCoords;
    while (std::getline(coordFile, line)) {
        std::stringstream ss(line);
        double x, y;
        char comma;
        ss >> x >> comma >> y;
        nodeCoords.push_back(std::make_pair(x, y));
    }
    coordFile.close();

    int numNodes = adjMatrix.size();

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Configure point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create net device containers for each link
    NetDeviceContainer deviceContainer[numNodes][numNodes];

    // Connect nodes based on adjacency matrix
    for (int i = 0; i < numNodes; ++i) {
        for (int j = i + 1; j < numNodes; ++j) {
            if (adjMatrix[i][j] == 1) {
                NodeContainer linkNodes;
                linkNodes.Add(nodes.Get(i));
                linkNodes.Add(nodes.Get(j));
                deviceContainer[i][j] = pointToPoint.Install(linkNodes);
                deviceContainer[j][i] = deviceContainer[i][j];
            }
        }
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);

    // Set up CBR traffic
    uint16_t cbrPort = 12345;
    for (int i = 0; i < numNodes; ++i) {
        for (int j = 0; j < numNodes; ++j) {
            if (i != j) {
                // Create CBR application on node i sending to node j
                ConstantRateWifiMacApplicationHelper cbrSourceHelper;
                cbrSourceHelper.SetStreamAttribute ("RemoteAddress", AddressValue (InetSocketAddress (interfaces.GetAddress (j), cbrPort)));
                cbrSourceHelper.SetStreamAttribute ("PacketSize", UintegerValue (1024));
                cbrSourceHelper.SetStreamAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
                cbrSourceHelper.SetStreamAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
                cbrSourceHelper.SetStreamAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
                ApplicationContainer sourceApps = cbrSourceHelper.Install (nodes.Get (i));
                sourceApps.Start (Seconds (1.0));
                sourceApps.Stop (Seconds (10.0));
            }
        }
    }

    // Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    for (int i = 0; i < numNodes; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<MobilityModel> mobilityModel = node->GetObject<MobilityModel>();
        mobilityModel->SetPosition(Vector(nodeCoords[i].first, nodeCoords[i].second, 0.0));
    }

    // Animation
    AnimationInterface anim("wired_network.xml");
    for (int i = 0; i < numNodes; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node " + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}