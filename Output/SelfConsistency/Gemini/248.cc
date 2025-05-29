#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopology");

int
main (int argc, char *argv[])
{
  LogComponent::SetEnable (
      "StarTopology", LOG_LEVEL_INFO);

  uint32_t nSpokes = 3; // Number of spoke nodes
  CommandLine cmd;
  cmd.AddValue ("nSpokes", "Number of spoke nodes", nSpokes);
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Creating " << nSpokes + 1 << " nodes.");
  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer spokeNodes;
  spokeNodes.Create (nSpokes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  for (uint32_t i = 0; i < nSpokes; ++i)
    {
      NetDeviceContainer devices = pointToPoint.Install (hubNode.Get (0), spokeNodes.Get (i));
      NS_LOG_INFO ("Created link between hub and spoke " << i);
    }

  NS_LOG_INFO ("Topology setup complete.");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}