#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  uint32_t numNodes = 5;

  CommandLine cmd;
  cmd.AddValue("numNodes", "Number of nodes in the mesh", numNodes);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t i = 0; i < numNodes; ++i) {
    for (uint32_t j = i + 1; j < numNodes; ++j) {
      NetDeviceContainer devices = pointToPoint.Install(nodes.Get(i), nodes.Get(j));
      std::cout << "Created link between node " << i << " and node " << j << std::endl;
    }
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}