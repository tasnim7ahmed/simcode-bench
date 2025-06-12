#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.AddValue("adjacencyMatrixFile", "File containing the adjacency matrix", "adjacency_matrix.txt");
  cmd.AddValue("nodeCoordinatesFile", "File containing the node coordinates", "node_coordinates.txt");
  cmd.Parse(argc, argv);

  std::string adjacencyMatrixFile = "adjacency_matrix.txt";
  std::string nodeCoordinatesFile = "node_coordinates.txt";

  cmd.Parse(argc, argv);

  std::ifstream adjacencyFile(adjacencyMatrixFile);
  std::ifstream coordinatesFile(nodeCoordinatesFile);

  if (!adjacencyFile.is_open() || !coordinatesFile.is_open()) {
    std::cerr << "Error: Could not open input files." << std::endl;
    return 1;
  }

  std::vector<std::vector<int>> adjacencyMatrix;
  std::string line;
  while (std::getline(adjacencyFile, line)) {
    std::vector<int> row;
    std::stringstream ss(line);
    int value;
    char comma;
    while (ss >> value) {
      row.push_back(value);
      ss >> comma; // Consume the comma
    }
    adjacencyMatrix.push_back(row);
  }

  std::vector<std::pair<double, double>> nodeCoordinates;
  while (std::getline(coordinatesFile, line)) {
    std::stringstream ss(line);
    double x, y;
    char comma;
    ss >> x >> comma >> y;
    nodeCoordinates.push_back(std::make_pair(x, y));
  }

  uint32_t numNodes = adjacencyMatrix.size();
  if (numNodes == 0 || numNodes != nodeCoordinates.size()) {
    std::cerr << "Error: Invalid input data." << std::endl;
    return 1;
  }

  NodeContainer nodes;
  nodes.Create(numNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  std::vector<NetDeviceContainer> deviceContainers;
  for (uint32_t i = 0; i < numNodes; ++i) {
    deviceContainers.push_back(NetDeviceContainer());
  }

  for (uint32_t i = 0; i < numNodes; ++i) {
    for (uint32_t j = i + 1; j < numNodes; ++j) {
      if (adjacencyMatrix[i][j]) {
        NetDeviceContainer devices = pointToPoint.Install(nodes.Get(i), nodes.Get(j));
        deviceContainers[i].Add(devices.Get(0));
        deviceContainers[j].Add(devices.Get(1));
      }
    }
  }

  Ipv4InterfaceContainer interfaces;
  for(uint32_t i = 0; i < numNodes; ++i){
    interfaces = address.Assign(deviceContainers[i]);
    address.NewNetwork();
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  for (uint32_t i = 0; i < numNodes; ++i) {
    for (uint32_t j = 0; j < numNodes; ++j) {
      if (i != j) {
        Address sinkAddress(InetSocketAddress(interfaces.GetAddress(j, 0), port));
        PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
        ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(j));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        OnOffHelper onOffHelper("ns3::UdpSocketFactory", sinkAddress);
        onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));
      }
    }
  }

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  for (uint32_t i = 0; i < numNodes; ++i) {
    Ptr<ConstantPositionMobilityModel> mob = nodes.Get(i)->GetObject<ConstantPositionMobilityModel>();
    mob->SetPosition(Vector(nodeCoordinates[i].first, nodeCoordinates[i].second, 0));
  }

  AnimationInterface anim("wired-network.xml");
  for(uint32_t i = 0; i < numNodes; ++i){
      anim.UpdateNodeDescription(nodes.Get(i), "Node " + std::to_string(i));
      anim.UpdateNodeSize(nodes.Get(i)->GetId(), 1.0, 1.0);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}