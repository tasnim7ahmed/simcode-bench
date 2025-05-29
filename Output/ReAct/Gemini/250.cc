#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nNodes = 5;

  NodeContainer nodes;
  nodes.Create(nNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t i = 0; i < nNodes; ++i) {
    for (uint32_t j = i + 1; j < nNodes; ++j) {
      NetDeviceContainer devices = pointToPoint.Install(nodes.Get(i), nodes.Get(j));
      NS_LOG_INFO("Created link between node " << i << " and node " << j);
    }
  }

  Simulator::RunSeconds(1.0);
  Simulator::Destroy();
  return 0;
}