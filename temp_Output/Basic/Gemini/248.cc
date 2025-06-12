#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  NodeContainer hubNode = NodeContainer(nodes.Get(0));
  NodeContainer spokeNodes;
  for (uint32_t i = 1; i < 5; ++i) {
    spokeNodes.Add(nodes.Get(i));
  }

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t i = 0; i < spokeNodes.GetN(); ++i) {
    NetDeviceContainer devices = pointToPoint.Install(hubNode.Get(0), spokeNodes.Get(i));
    std::cout << "Connected hub node to spoke node " << i + 1 << std::endl;
  }

  std::cout << "Star topology created successfully." << std::endl;

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}