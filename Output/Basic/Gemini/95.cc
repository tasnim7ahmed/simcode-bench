#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdjacencyMatrixNetwork");

int main(int argc, char *argv[]) {
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetLogComponentEnable("AdjacencyMatrixNetwork", LOG_LEVEL_INFO);

  std::string adjacencyMatrixFile = "adjacency_matrix.txt";
  std::string nodeCoordinatesFile = "node_coordinates.txt";
  std::string animFile = "adjacency-matrix-network.xml";

  CommandLine cmd;
  cmd.AddValue("adjacencyMatrixFile", "Adjacency matrix file name", adjacencyMatrixFile);
  cmd.AddValue("nodeCoordinatesFile", "Node coordinates file name", nodeCoordinatesFile);
  cmd.AddValue("animFile", "NetAnim output file", animFile);
  cmd.Parse(argc, argv);

  std::ifstream adjFile(adjacencyMatrixFile);
  if (!adjFile.is_open()) {
    std::cerr << "Error opening adjacency matrix file: " << adjacencyMatrixFile << std::endl;
    return 1;
  }

  std::vector<std::vector<int>> adjacencyMatrix;
  std::string line;
  while (std::getline(adjFile, line)) {
    std::vector<int> row;
    std::stringstream ss(line);
    int value;
    char delimiter;
    while (ss >> value) {
      row.push_back(value);
      ss >> delimiter; // Attempt to read the delimiter (e.g., ',')
    }
    adjacencyMatrix.push_back(row);
  }
  adjFile.close();

  int numNodes = adjacencyMatrix.size();
  std::cout << "Number of Nodes: " << numNodes << std::endl;

  NodeContainer nodes;
  nodes.Create(numNodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[numNodes][numNodes];

  for (int i = 0; i < numNodes; ++i) {
    for (int j = i + 1; j < numNodes; ++j) {
      if (adjacencyMatrix[i][j]) {
        NetDeviceContainer pair = p2p.Install(nodes.Get(i), nodes.Get(j));
        devices[i][j] = pair;
        devices[j][i] = pair;
        std::cout << "Link created between Node " << i << " and Node " << j << std::endl;
      }
    }
  }

  Ipv4AddressHelper address;
  address.SetBase("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;

  for (int i = 0; i < numNodes; ++i) {
    for (int j = i + 1; j < numNodes; ++j) {
      if (adjacencyMatrix[i][j]) {
        address.Assign(devices[i][j]);
        interfaces.Add(devices[i][j].Get(0)->GetIf());
        interfaces.Add(devices[i][j].Get(1)->GetIf());
      }
    }
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // CBR Traffic
  uint16_t port = 9;
  for (int i = 0; i < numNodes; ++i) {
    for (int j = 0; j < numNodes; ++j) {
      if (i != j) {
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(j), port));
        ApplicationContainer sinkApps = sink.Install(nodes.Get(j));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
        var->SetAttribute("Stream", IntegerValue(i * numNodes + j));
        
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(j), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        ApplicationContainer clientApps = onoff.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));
      }
    }
  }

  // Node Coordinates for NetAnim
  std::ifstream coordFile(nodeCoordinatesFile);
  if (!coordFile.is_open()) {
    std::cerr << "Error opening node coordinates file: " << nodeCoordinatesFile << std::endl;
    return 1;
  }

  double x, y;
  for (int i = 0; i < numNodes; ++i) {
    coordFile >> x >> y;
    AnimationInterface::SetConstantPosition(nodes.Get(i), x, y);
  }
  coordFile.close();

  AnimationInterface anim(animFile);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::ofstream os;
  os.open("adjacency_matrix_network.flowmon");
  monitor->SerializeToXmlFile("adjacency_matrix_network.flowmon", false, true);

  Simulator::Destroy();
  return 0;
}