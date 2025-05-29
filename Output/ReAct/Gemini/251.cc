#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t numNodes = 5;

  NodeContainer nodes;
  nodes.Create(numNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t i = 0; i < numNodes; ++i) {
    std::cout << "Node " << i << " created" << std::endl;
  }

  for (uint32_t i = 0; i < numNodes; ++i) {
    uint32_t nextNode = (i + 1) % numNodes;
    NetDeviceContainer devices = pointToPoint.Install(nodes.Get(i), nodes.Get(nextNode));
    std::cout << "Connection between Node " << i << " and Node " << nextNode << " created." << std::endl;
  }

  std::cout << "Ring topology created successfully." << std::endl;

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}