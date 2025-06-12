#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LinearTopology");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (LogComponent::GetComponent ("LinearTopology"), LOG_LEVEL_INFO);

  uint32_t numNodes = 5;

  NodeContainer nodes;
  nodes.Create (numNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      NetDeviceContainer devices = pointToPoint.Install (nodes.Get (i), nodes.Get (i + 1));
      NS_LOG_INFO ("Created link between node " << i << " and node " << i + 1);
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}