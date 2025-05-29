#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiredNetworkSimulation");

int main(int argc, char* argv[]) {
    LogComponent::Enable("UdpClient", LOG_LEVEL_INFO);
    LogComponent::Enable("UdpServer", LOG_LEVEL_INFO);

    std::string adjacencyMatrixFile = "adjacency_matrix.txt";
    std::string nodeCoordinatesFile = "node_coordinates.txt";
    double simulationTime = 10.0;
    std::string animFile = "wired-network.xml";

    CommandLine cmd;
    cmd.AddValue("adjacencyMatrix", "Path to the adjacency matrix file", adjacencyMatrixFile);
    cmd.AddValue("nodeCoordinates", "Path to the node coordinates file", nodeCoordinatesFile);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("animFile", "NetAnim output file", animFile);
    cmd.Parse(argc, argv);

    // Read adjacency matrix from file
    std::ifstream adjFile(adjacencyMatrixFile);
    if (!adjFile.is_open()) {
        std::cerr << "Error: Could not open adjacency matrix file: " << adjacencyMatrixFile << std::endl;
        return 1;
    }

    std::vector<std::vector<int>> adjacencyMatrix;
    std::string line;
    while (std::getline(adjFile, line)) {
        std::vector<int> row;
        std::stringstream ss(line);
        int value;
        while (ss >> value) {
            row.push_back(value);
        }
        adjacencyMatrix.push_back(row);
    }
    adjFile.close();

    int numNodes = adjacencyMatrix.size();

    // Read node coordinates from file
    std::ifstream coordFile(nodeCoordinatesFile);
    if (!coordFile.is_open()) {
        std::cerr << "Error: Could not open node coordinates file: " << nodeCoordinatesFile << std::endl;
        return 1;
    }

    std::vector<std::pair<double, double>> nodeCoordinates;
    while (std::getline(coordFile, line)) {
        std::stringstream ss(line);
        double x, y;
        char comma;
        if (ss >> x >> comma >> y) {
            nodeCoordinates.push_back(std::make_pair(x, y));
        } else {
            std::cerr << "Error: Invalid format in node coordinates file" << std::endl;
            return 1;
        }
    }
    coordFile.close();

    if (numNodes != nodeCoordinates.size()) {
        std::cerr << "Error: Number of nodes in adjacency matrix and node coordinates file do not match." << std::endl;
        return 1;
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Configure P2P links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[numNodes][numNodes];
    Ipv4InterfaceContainer interfaces[numNodes][numNodes];

    // Create links based on adjacency matrix
    for (int i = 0; i < numNodes; ++i) {
        for (int j = i + 1; j < numNodes; ++j) {
            if (adjacencyMatrix[i][j] == 1) {
                NetDeviceContainer linkDevices = p2p.Install(nodes.Get(i), nodes.Get(j));
                devices[i][j] = linkDevices;
                devices[j][i] = linkDevices; // For ease of access later
            }
        }
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    uint32_t interfaceCounter = 0;

    for (int i = 0; i < numNodes; ++i) {
        for (int j = i + 1; j < numNodes; ++j) {
            if (adjacencyMatrix[i][j] == 1) {
                Ipv4InterfaceContainer linkInterfaces = address.Assign(devices[i][j]);
                interfaces[i][j] = linkInterfaces;
                interfaces[j][i] = linkInterfaces; // For ease of access later
                address.NewNetwork();
                interfaceCounter += 2;
            }
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure CBR traffic flows
    uint16_t port = 9;
    for (int i = 0; i < numNodes; ++i) {
        for (int j = 0; j < numNodes; ++j) {
            if (i != j) {
                // Source and sink nodes
                Address sinkAddress(InetSocketAddress(interfaces[std::min(i, j)][std::max(i, j)].GetAddress(i < j ? 1 : 0), port)); // Correct interface selection
                PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
                ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(j)); // Sink at destination
                sinkApps.Start(Seconds(0.0));
                sinkApps.Stop(Seconds(simulationTime));

                Ptr<Socket> ns3UdpSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
                Ptr<UdpClient> client = CreateObject<UdpClient>();
                client->SetAttribute("RemoteAddress", AddressValue(sinkAddress));
                client->SetAttribute("RemotePort", UintegerValue(port));
                client->SetAttribute("PacketSize", UintegerValue(1024));
                client->SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited packets
                client->SetAttribute("Interval", TimeValue(Seconds(0.01))); // Send every 0.01 seconds
                nodes.Get(i)->AddApplication(client);
                client->SetStartTime(Seconds(1.0)); // Start sending at 1 second
                client->SetStopTime(Seconds(simulationTime));

                port++; // Increment port for each flow
            }
        }
    }

    // Enable flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable NetAnim
    AnimationInterface anim(animFile);
    for (int i = 0; i < numNodes; ++i) {
        anim.SetConstantPosition(nodes.Get(i), nodeCoordinates[i].first, nodeCoordinates[i].second);
    }

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

    monitor->SerializeToXmlFile("wired-network.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}