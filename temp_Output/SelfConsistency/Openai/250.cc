#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Set number of nodes in the mesh
  uint32_t nNodes = 4;

  // Command line argument for flexibility
  CommandLine cmd (__FILE__);
  cmd.AddValue ("nNodes", "Number of nodes in the mesh", nNodes);
  cmd.Parse (argc, argv);

  if (nNodes < 2)
    {
      NS_LOG_UNCOND ("Need at least 2 nodes to form a mesh.");
      return 1;
    }

  // Create nNodes nodes
  NodeContainer nodes;
  nodes.Create (nNodes);

  // Set up point-to-point attributes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect every pair of nodes (i, j), i < j
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      for (uint32_t j = i + 1; j < nNodes; ++j)
        {
          NodeContainer pair (nodes.Get(i), nodes.Get(j));
          NetDeviceContainer devices = p2p.Install (pair);
          NS_LOG_UNCOND ("Established point-to-point link between Node " << i << " and Node " << j);
        }
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}