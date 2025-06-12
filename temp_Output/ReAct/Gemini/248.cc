#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nSpokes = 3;

  NodeContainer hubNode;
  hubNode.Create(1);

  NodeContainer spokeNodes;
  spokeNodes.Create(nSpokes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t i = 0; i < nSpokes; ++i) {
    NetDeviceContainer devices = pointToPoint.Install(hubNode.Get(0), spokeNodes.Get(i));
    std::cout << "Created link between hub and spoke " << i << std::endl;
  }

  Simulator::RunSeconds(1.0);
  Simulator::Destroy();
  return 0;
}