/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
#include <iostream>

using namespace ns3;

std::vector<std::vector<int>>
ReadAdjacencyMatrix (const std::string& filename, uint32_t& numNodes)
{
  std::ifstream infile (filename);
  std::vector<std::vector<int>> matrix;
  std::string line;
  numNodes = 0;

  // First, read lines and parse into vectors
  while (std::getline (infile, line))
    {
      if (line.empty ())
        continue;
      std::vector<int> row;
      std::istringstream iss (line);
      int val;
      while (iss >> val)
        {
          row.push_back (val);
        }
      matrix.push_back (row);
      numNodes++;
    }

  // Sanity check for upper triangular
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      if (matrix[i].size () != numNodes)
        {
          NS_FATAL_ERROR ("Matrix is not square!");
        }
    }
  return matrix;
}

std::vector<std::pair<double, double>>
ReadNodeCoordinates (const std::string& filename, uint32_t expectedNodes)
{
  std::ifstream infile (filename);
  std::string line;
  std::vector<std::pair<double, double>> coords;

  while (std::getline (infile, line))
    {
      if (line.empty ())
        continue;
      std::istringstream iss (line);
      double x, y;
      if (!(iss >> x >> y))
        {
          NS_FATAL_ERROR ("Invalid coordinate format in " << filename);
        }
      coords.emplace_back (x, y);
    }
  if (coords.size () != expectedNodes)
    {
      NS_FATAL_ERROR ("Coordinate count (" << coords.size () << ") != node count (" << expectedNodes << ")");
    }
  return coords;
}

int
main (int argc, char *argv[])
{
  std::string adjacencyMatrixFile = "adjacency_matrix.txt";
  std::string nodeCoordinatesFile = "node_coordinates.txt";
  std::string animFile = "wired-netanim.xml";
  uint32_t numNodes = 0;
  CommandLine cmd;
  cmd.AddValue ("adjacencyMatrix", "Input adjacency matrix filename (upper triangular)", adjacencyMatrixFile);
  cmd.AddValue ("nodeCoordinates", "Input node coordinates filename", nodeCoordinatesFile);
  cmd.AddValue ("animFile", "NetAnim output filename", animFile);
  cmd.Parse (argc, argv);

  // STEP 1: Read topology
  std::vector<std::vector<int>> adjacencyMatrix = ReadAdjacencyMatrix (adjacencyMatrixFile, numNodes);
  std::vector<std::pair<double, double>> nodeCoords = ReadNodeCoordinates (nodeCoordinatesFile, numNodes);

  // STEP 2: Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // STEP 3: Links per adjacency matrix - for each (i,j) i<j where adjacencyMatrix[i][j]==1
  std::vector<std::pair<uint32_t, uint32_t>> links;
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      for (uint32_t j = i + 1; j < numNodes; ++j)
        {
          if (adjacencyMatrix[i][j] == 1)
            {
              links.emplace_back (i, j);
            }
        }
    }

  // Map each point-to-point NetDevice/Interface back to node pairs
  std::vector<NetDeviceContainer> netDeviceContainers;
  std::vector<NodeContainer> linkNodeContainers;

  // STEP 4: Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  for (auto& link : links)
    {
      NodeContainer pair (nodes.Get (link.first), nodes.Get (link.second));
      NetDeviceContainer nd = p2p.Install (pair);
      netDeviceContainers.push_back (nd);
      linkNodeContainers.push_back (pair);
    }

  // STEP 5: Install internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // STEP 6: Assign IP addresses per link
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaceContainers;
  uint32_t subnetId = 1;
  for (size_t linkIdx = 0; linkIdx < links.size (); ++linkIdx)
    {
      std::ostringstream subnet;
      subnet << "10." << subnetId << ".1.0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      Ipv4InterfaceContainer iface = address.Assign (netDeviceContainers[linkIdx]);
      interfaceContainers.push_back (iface);
      subnetId++;
    }

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // STEP 7: Setup CBR traffic flows from every node to every other node (n*(n-1) flows)
  uint16_t portBase = 8000;
  double appStart = 1.0;
  double appStop = 9.0;
  uint32_t pktSize = 512;
  std::string rateStr = "1Mbps";

  for (uint32_t src = 0; src < numNodes; ++src)
    {
      for (uint32_t dst = 0; dst < numNodes; ++dst)
        {
          if (src == dst)
            continue;

          // Find target node's IP address on one of its interfaces
          Ptr<Node> dstNode = nodes.Get (dst);
          Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4> ();
          Ipv4Address dstAddr;
          // Find primary address (interface 1 or higher; 0 is loopback)
          bool found = false;
          for (uint32_t ifidx = 1; ifidx < ipv4->GetNInterfaces (); ++ifidx)
            {
              for (uint32_t adxidx = 0; adxidx < ipv4->GetNAddresses (ifidx); ++adxidx)
                {
                  Ipv4InterfaceAddress addr = ipv4->GetAddress (ifidx, adxidx);
                  if (addr.GetLocal () != Ipv4Address ("127.0.0.1"))
                    {
                      dstAddr = addr.GetLocal ();
                      found = true;
                      break;
                    }
                }
              if (found) break;
            }

          if (!found)
            {
              NS_FATAL_ERROR ("No non-loopback address on node " << dst);
            }

          // Install PacketSink on destination
          uint16_t port = portBase + src;
          Address sinkAddress (InetSocketAddress (dstAddr, port));
          PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
          ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (dst));
          sinkApps.Start (Seconds (0.0));
          sinkApps.Stop (Seconds (appStop + 1.0));

          // Install OnOffApplication on source
          OnOffHelper onoff ("ns3::TcpSocketFactory", sinkAddress);
          onoff.SetAttribute ("DataRate", StringValue (rateStr));
          onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
          onoff.SetAttribute ("StartTime", TimeValue (Seconds (appStart)));
          onoff.SetAttribute ("StopTime", TimeValue (Seconds (appStop)));
          ApplicationContainer srcApps = onoff.Install (nodes.Get (src));
        }
    }

  // STEP 8: Set up NetAnim
  AnimationInterface anim (animFile);

  // Set node positions
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      anim.SetConstantPosition (nodes.Get (i), nodeCoords[i].first, nodeCoords[i].second);
    }

  // Optionally, label nodes
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      std::ostringstream oss;
      oss << "Node-" << i;
      anim.UpdateNodeDescription (nodes.Get (i), oss.str ());
      anim.UpdateNodeColor (nodes.Get (i), 50 + i * 30, 100, 255 - (i * 20));
    }

  // Enable link coloring (optional)
  for (size_t linkIdx = 0; linkIdx < linkNodeContainers.size (); ++linkIdx)
    {
      // Just alternate colors for visual clarity
      uint8_t r = 100 + ((linkIdx * 50) % 156);
      uint8_t g = 80;
      uint8_t b = 255 - ((linkIdx * 60) % 200);
      anim.UpdateLinkDescription (linkNodeContainers[linkIdx].Get (0), linkNodeContainers[linkIdx].Get (1),
                                 "P2P");
      anim.UpdateLinkColor (linkNodeContainers[linkIdx].Get (0), linkNodeContainers[linkIdx].Get (1),
                            r, g, b, 255);
    }

  // STEP 9: Enable logging to stdout for sinks (optional)
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  Simulator::Stop (Seconds (appStop + 2.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}