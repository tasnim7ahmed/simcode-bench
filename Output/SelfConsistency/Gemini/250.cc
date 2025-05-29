#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshExample");

int main() {
  LogComponent::SetLevel( "MeshExample", LOG_LEVEL_INFO );

  uint32_t numNodes = 5;

  NodeContainer nodes;
  nodes.Create(numNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[numNodes][numNodes];

  for (uint32_t i = 0; i < numNodes; ++i) {
    for (uint32_t j = i + 1; j < numNodes; ++j) {
      NetDeviceContainer link = pointToPoint.Install(nodes.Get(i), nodes.Get(j));
      devices[i][j] = link;
      devices[j][i] = link;

      NS_LOG_INFO("Created link between node " << i << " and node " << j);
    }
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}