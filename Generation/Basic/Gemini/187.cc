#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include "ns3/mobility-module.h" // For setting node positions in NetAnim

#include <vector>
#include <random> // For random server selection
#include <map>

// Required for the ns-3 namespace
using namespace ns3;

// Define a logging component for this simulation script
NS_LOG_COMPONENT_DEFINE ("FatTreeSimulation");

int main (int argc, char *argv[])
{
  // Default simulation parameters
  uint32_t k = 4; // K for K-ary Fat-Tree (must be an even number)
  double simTime = 10.0; // Total simulation time in seconds
  uint32_t numFlows = 10; // Number of TCP BulkSend flows to establish

  // Parse command line arguments
  CommandLine cmd(__FILE__);
  cmd.AddValue ("k", "K for K-ary Fat-Tree. Must be an even number.", k);
  cmd.AddValue ("simTime", "Total simulation time in seconds.", simTime);
  cmd.AddValue ("numFlows", "Number of TCP flows to establish.", numFlows);
  cmd.Parse (argc, argv);

  // Validate K
  if (k == 0 || k % 2 != 0)
    {
      NS_FATAL_ERROR ("K must be a non-zero even number for a Fat-Tree topology.");
    }

  // Configure TCP properties (e.g., using TcpNewReno, larger buffers)
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  // Increase TCP send and receive buffer sizes
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 20)); // 1 MB
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 20)); // 1 MB
  // Increase DropTailQueue limits to reduce drops under high load
  Config::SetDefault ("ns3::DropTailQueue::MaxBytes", UintegerValue (100 * 1500)); // ~100 packets
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (1000)); // Or 1000 packets

  // Enable logging for specific components to observe simulation events
  LogComponentEnable ("FatTreeSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  // LogComponentEnable ("TcpSocketBase", LOG_LEVEL_INFO); // Uncomment for verbose TCP logging
  // LogComponentEnable ("Ipv4GlobalRouting", LOG_LEVEL_INFO); // Uncomment for verbose routing logging

  // 1. Node Creation
  // Core Layer: (k/2)^2 core switches
  NodeContainer coreSwitches;
  coreSwitches.Create ((k/2)*(k/2));

  // Aggregation and Edge Layers: k pods, each with k agg and k edge switches
  std::vector<NodeContainer> aggSwitchesInPod(k);
  std::vector<NodeContainer> edgeSwitchesInPod(k);
  // Server Layer: k pods, each with k edge switches, each with k/2 servers
  std::vector<std::vector<NodeContainer>> serversAttachedToEdge(k);

  for (uint32_t p = 0; p < k; ++p) // Iterate over pods
    {
      aggSwitchesInPod[p].Create(k);
      edgeSwitchesInPod[p].Create(k);
      serversAttachedToEdge[p].resize(k); // Each pod has 'k' edge switches
      for (uint32_t e = 0; e < k; ++e) // Iterate over edge switches within a pod
        {
          serversAttachedToEdge[p][e].Create(k/2); // Each edge switch connects to k/2 servers
        }
    }

  // Install Internet Stack (TCP/IP) on all nodes
  InternetStackHelper internet;
  internet.InstallAll (); // This installs on all nodes that have been created up to this point

  // 2. Device Installation and IP Addressing
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps")); // 100 Mbps links
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));      // 1 ms propagation delay

  Ipv4AddressHelper ipv4;
  uint32_t ipSubnetCounter = 0; // Counter for /30 IP subnets (increments by 4 for each subnet)

  // Store all server nodes and their IP addresses for later flow creation
  std::vector<Ptr<Node>> allServers;
  std::map<Ptr<Node>, Ipv4Address> serverIps; // Map node pointer to its primary IP

  // 2.1 Core to Aggregation Connections
  NS_LOG_INFO ("Creating Core-Aggregation links.");
  for (uint32_t c = 0; c < (k/2)*(k/2); ++c) // Iterate through core switches
    {
      for (uint32_t p = 0; p < k; ++p) // Iterate through pods (connects to one agg switch in each pod)
        {
          // Connect core switch 'c' to aggregation switch '(c / (k/2))' in pod 'p'
          NetDeviceContainer link = p2p.Install (coreSwitches.Get(c), aggSwitchesInPod[p].Get(c / (k/2)));
          ipv4.SetBase (Ipv4Address ("10.0.0.0").GetSubnetAddress(ipSubnetCounter * 4), "255.255.255.252"); // /30 subnet
          ipv4.Assign (link);
          ipSubnetCounter++;
        }
    }

  // 2.2 Aggregation to Edge Connections (within each pod)
  NS_LOG_INFO ("Creating Aggregation-Edge links.");
  for (uint32_t p = 0; p < k; ++p) // Iterate through pods
    {
      for (uint32_t a = 0; a < k; ++a) // Iterate through aggregation switches in current pod
        {
          for (uint32_t e_offset = 0; e_offset < k/2; ++e_offset) // Each agg connects to k/2 edge switches
            {
              // Connect agg switch 'a' to edge switch '((a + e_offset) % k)' in pod 'p'
              NetDeviceContainer link = p2p.Install (aggSwitchesInPod[p].Get(a), edgeSwitchesInPod[p].Get((a + e_offset) % k));
              ipv4.SetBase (Ipv4Address ("10.0.0.0").GetSubnetAddress(ipSubnetCounter * 4), "255.255.255.252"); // /30 subnet
              ipv4.Assign (link);
              ipSubnetCounter++;
            }
        }
    }

  // 2.3 Edge to Server Connections (within each pod, for each edge switch)
  NS_LOG_INFO ("Creating Edge-Server links.");
  for (uint32_t p = 0; p < k; ++p) // Iterate through pods
    {
      for (uint32_t e = 0; e < k; ++e) // Iterate through edge switches in current pod
        {
          for (uint32_t s = 0; s < k/2; ++s) // Each edge connects to k/2 servers
            {
              NetDeviceContainer link = p2p.Install (edgeSwitchesInPod[p].Get(e), serversAttachedToEdge[p][e].Get(s));
              ipv4.SetBase (Ipv4Address ("10.0.0.0").GetSubnetAddress(ipSubnetCounter * 4), "255.255.255.252"); // /30 subnet
              Ipv4InterfaceContainer interfaces = ipv4.Assign (link);
              ipSubnetCounter++;
              // Store server node and its assigned IP (server side of the link)
              allServers.push_back(serversAttachedToEdge[p][e].Get(s));
              serverIps[serversAttachedToEdge[p][e].Get(s)] = interfaces.GetAddress(1); // Get IP of the second device (server)
            }
        }
    }

  // 3. Populate Routing Tables
  // Ipv4GlobalRoutingHelper will automatically configure static routes on all nodes
  // based on the discovered topology.
  NS_LOG_INFO ("Populating routing tables using Ipv4GlobalRoutingHelper.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // 4. Create TCP Applications (BulkSend)
  NS_LOG_INFO ("Creating " << numFlows << " TCP applications.");
  
  // Setup random number generation for selecting sender/receiver servers
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(0, allServers.size() - 1);

  for (uint32_t flowId = 0; flowId < numFlows; ++flowId)
    {
      Ptr<Node> senderNode = allServers[distrib(gen)];
      Ptr<Node> receiverNode = allServers[distrib(gen)];

      // Ensure sender and receiver are different nodes
      while (senderNode == receiverNode)
        {
          receiverNode = allServers[distrib(gen)];
        }

      // Get receiver's IP address from the map
      Ipv4Address receiverAddress = serverIps.at(receiverNode);

      // Create PacketSink (receiver) application
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                   InetSocketAddress (Ipv4Address::GetAny (), 9000 + flowId)); // Use unique port for each flow
      ApplicationContainer sinkApps = sinkHelper.Install (receiverNode);
      sinkApps.Start (Seconds (0.0)); // Sink starts at simulation beginning
      sinkApps.Stop (Seconds (simTime)); // Sink stops at simulation end

      // Create BulkSend (sender) application
      BulkSendHelper sourceHelper ("ns3::TcpSocketFactory",
                                   InetSocketAddress (receiverAddress, 9000 + flowId));
      // MaxBytes=0 means send data until the application is stopped
      sourceHelper.SetAttribute ("MaxBytes", UintegerValue (0));
      ApplicationContainer sourceApps = sourceHelper.Install (senderNode);
      sourceApps.Start (Seconds (1.0 + (double)flowId * 0.1)); // Stagger start times to avoid initial congestion
      sourceApps.Stop (Seconds (simTime - 0.1)); // Stop just before simulation end
    }
  
  // 5. NetAnim Visualization Setup
  NS_LOG_INFO ("Enabling NetAnim visualization to fat-tree.xml.");
  AnimationInterface anim ("fat-tree.xml");
  // Set node positions for better visualization in NetAnim
  // Core switches at the top
  double coreXOffset = 50.0;
  double coreY = 10.0;
  double coreSpacing = 30.0; // Spacing for core switches in a grid
  for (uint32_t i = 0; i < (k/2); ++i) { // rows
      for (uint32_t j = 0; j < (k/2); ++j) { // columns
          uint32_t c_idx = i * (k/2) + j;
          anim.SetConstantPosition (coreSwitches.Get(c_idx), coreXOffset + j * coreSpacing, coreY + i * coreSpacing);
      }
  }

  // Pods (Aggregation, Edge, Servers) arranged horizontally
  double podStartX = 0.0;
  double podSpacingX = 100.0; // Horizontal spacing between pods
  double aggY = 100.0;       // Y-coordinate for aggregation layer
  double edgeY = 200.0;      // Y-coordinate for edge layer
  double serverY = 300.0;    // Y-coordinate for server layer
  double switchSpacingInPodX = 20.0; // Horizontal spacing between switches within a pod
  double serverSpacingInEdgeX = 10.0; // Horizontal spacing between servers on one edge

  for (uint32_t p = 0; p < k; ++p) // Iterate over pods
    {
      // Aggregation layer nodes for current pod
      for (uint32_t a = 0; a < k; ++a)
        {
          anim.SetConstantPosition (aggSwitchesInPod[p].Get(a), 
                                    podStartX + p * podSpacingX + a * switchSpacingInPodX, aggY);
        }
      
      // Edge layer nodes for current pod
      for (uint32_t e = 0; e < k; ++e)
        {
          anim.SetConstantPosition (edgeSwitchesInPod[p].Get(e), 
                                    podStartX + p * podSpacingX + e * switchSpacingInPodX, edgeY);
          
          // Server nodes connected to this edge switch
          for (uint32_t s = 0; s < k/2; ++s)
            {
              anim.SetConstantPosition (serversAttachedToEdge[p][e].Get(s), 
                                        podStartX + p * podSpacingX + e * switchSpacingInPodX + s * serverSpacingInEdgeX, serverY);
            }
        }
    }

  // 6. Simulation Execution
  NS_LOG_INFO ("Running simulation for " << simTime << " seconds.");
  Simulator::Stop (Seconds (simTime)); // Set the total simulation duration
  Simulator::Run ();                    // Start the simulation
  Simulator::Destroy ();                // Clean up resources after simulation
  NS_LOG_INFO ("Done.");

  return 0;
}