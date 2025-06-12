#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.AddValue("adjacencyMatrixFile", "Adjacency matrix file name", "adjacency_matrix.txt");
  cmd.AddValue("nodeCoordinatesFile", "Node coordinates file name", "node_coordinates.txt");
  cmd.Parse(argc, argv);

  std::string adjacencyMatrixFile = "adjacency_matrix.txt";
  std::string nodeCoordinatesFile = "node_coordinates.txt";

  cmd.Parse(argc, argv);

  // Read adjacency matrix from file
  std::ifstream adjFile(adjacencyMatrixFile);
  std::vector<std::vector<int>> adjacencyMatrix;
  std::string line;
  while (std::getline(adjFile, line)) {
    std::stringstream ss(line);
    int value;
    std::vector<int> row;
    while (ss >> value) {
      row.push_back(value);
    }
    adjacencyMatrix.push_back(row);
  }
  adjFile.close();

  // Read node coordinates from file
  std::ifstream coordFile(nodeCoordinatesFile);
  std::vector<std::pair<double, double>> nodeCoordinates;
  while (std::getline(coordFile, line)) {
    std::stringstream ss(line);
    double x, y;
    ss >> x >> y;
    nodeCoordinates.push_back(std::make_pair(x, y));
  }
  coordFile.close();

  int numNodes = adjacencyMatrix.size();

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Create point-to-point helper
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Create network interfaces
  NetDeviceContainer devices[numNodes][numNodes];

  for (int i = 0; i < numNodes; ++i) {
    for (int j = i + 1; j < numNodes; ++j) {
      if (adjacencyMatrix[i][j] == 1) {
        NetDeviceContainer linkDevices = p2p.Install(nodes.Get(i), nodes.Get(j));
        devices[i][j] = linkDevices;
        devices[j][i] = linkDevices;
      }
    }
  }

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[numNodes][numNodes];

  int subnetIndex = 1;
  for (int i = 0; i < numNodes; ++i) {
    for (int j = i + 1; j < numNodes; ++j) {
      if (adjacencyMatrix[i][j] == 1) {
        address.NewNetwork();
        interfaces[i][j] = address.Assign(devices[i][j]);
        interfaces[j][i] = interfaces[i][j];
      }
    }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create CBR traffic flows
  uint16_t port = 9;
  for (int i = 0; i < numNodes; ++i) {
    for (int j = 0; j < numNodes; ++j) {
      if (i != j) {
        // Create OnOff application to send UDP datagrams
        OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces[std::min(i,j)][std::max(i,j)].GetAddress(1), port)));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        ApplicationContainer onoffApp = onoff.Install(nodes.Get(i));
        onoffApp.Start(Seconds(1.0));
        onoffApp.Stop(Seconds(10.0));

        // Create sink application to receive UDP datagrams
        UdpClientHelper sink(interfaces[std::min(i,j)][std::max(i,j)].GetAddress(1), port);
        sink.SetAttribute("MaxPackets", UintegerValue(1000000));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(j));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));
        port++;
      }
    }
  }

  // Set node positions for NetAnim
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  for (int i = 0; i < numNodes; ++i) {
    Ptr<Node> node = nodes.Get(i);
    Ptr<ConstantPositionMobilityModel> mobilityModel = node->GetObject<ConstantPositionMobilityModel>();
    mobilityModel->SetPosition(Vector(nodeCoordinates[i].first, nodeCoordinates[i].second, 0.0));
  }

  // Enable NetAnim
  AnimationInterface anim("wired-network.xml");
  anim.EnablePacketMetadata();

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}