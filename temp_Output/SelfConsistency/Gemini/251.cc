#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopology");

int
main (int argc, char *argv[])
{
  LogComponent::SetLogLevel (LogComponent::GetComponent ("RingTopology"), LOG_LEVEL_INFO);

  uint32_t numNodes = 5;

  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes in the ring", numNodes);
  cmd.Parse (argc, argv);

  NS_LOG_INFO ("Creating " << numNodes << " nodes.");
  NodeContainer nodes;
  nodes.Create (numNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NS_LOG_INFO ("Creating the ring topology.");
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      uint32_t nextNode = (i + 1) % numNodes;
      NetDeviceContainer devices = pointToPoint.Install (nodes.Get (i), nodes.Get (nextNode));
      NS_LOG_INFO ("Connected node " << i << " to node " << nextNode << ".");
    }

  NS_LOG_INFO ("Ring topology created successfully.");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}