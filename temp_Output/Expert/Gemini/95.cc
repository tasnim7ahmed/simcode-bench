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
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdjacencyMatrixSimulation");

int main (int argc, char *argv[])
{
  LogComponent::SetEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponent::SetEnable ("UdpServer", LOG_LEVEL_INFO);

  std::string adjacencyMatrixFile = "adjacency_matrix.txt";
  std::string nodeCoordinatesFile = "node_coordinates.txt";
  std::string animFile = "adjacency_matrix_simulation.xml";

  CommandLine cmd;
  cmd.AddValue ("adjacencyMatrix", "Path to the adjacency matrix file", adjacencyMatrixFile);
  cmd.AddValue ("nodeCoordinates", "Path to the node coordinates file", nodeCoordinatesFile);
  cmd.AddValue ("animFile", "File name for the NetAnim output", animFile);
  cmd.Parse (argc, argv);

  std::ifstream adjacencyFile (adjacencyMatrixFile);
  std::vector<std::vector<bool>> adjacencyMatrix;
  std::string line;
  while (std::getline (adjacencyFile, line))
  {
    std::vector<bool> row;
    std::stringstream ss (line);
    std::string value;
    while (std::getline (ss, value, ','))
    {
      row.push_back (value == "1");
    }
    adjacencyMatrix.push_back (row);
  }
  adjacencyFile.close ();

  int numNodes = adjacencyMatrix.size ();
  NodeContainer nodes;
  nodes.Create (numNodes);

  std::ifstream coordinatesFile (nodeCoordinatesFile);
  std::vector<Vector> nodePositions;
  while (std::getline (coordinatesFile, line))
  {
    std::stringstream ss (line);
    std::string xStr, yStr;
    std::getline (ss, xStr, ',');
    std::getline (ss, yStr, ',');
    double x = std::stod (xStr);
    double y = std::stod (yStr);
    nodePositions.push_back (Vector (x, y, 0));
  }
  coordinatesFile.close ();

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  for (int i = 0; i < numNodes; ++i)
  {
    Ptr<ConstantPositionMobilityModel> mobilityModel = nodes.Get (i)->GetObject<ConstantPositionMobilityModel> ();
    mobilityModel->SetPosition (nodePositions[i]);
  }

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  std::vector<NetDeviceContainer> deviceContainers(numNodes);

  for (int i = 0; i < numNodes; ++i) {
    deviceContainers[i].Create (numNodes);
  }
  
  for (int i = 0; i < numNodes; ++i)
  {
    for (int j = i + 1; j < numNodes; ++j)
    {
      if (adjacencyMatrix[i][j])
      {
          NetDeviceContainer devices = pointToPoint.Install (nodes.Get (i), nodes.Get (j));
          deviceContainers[i].Set(j,devices.Get(0));
          deviceContainers[j].Set(i,devices.Get(1));
      }
    }
  }

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer::Create(deviceContainers));
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  for (int i = 0; i < numNodes; ++i)
  {
    for (int j = 0; j < numNodes; ++j)
    {
      if (i != j)
      {
        UdpServerHelper server (port);
        ApplicationContainer serverApps = server.Install (nodes.Get (j));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        UdpClientHelper client (interfaces.GetAddress (j, 0), port);
        client.SetAttribute ("MaxPackets", UintegerValue (1000));
        client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
        client.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = client.Install (nodes.Get (i));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
        port++;
      }
    }
  }

  AnimationInterface anim (animFile);
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}