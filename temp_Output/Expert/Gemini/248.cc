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

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  for (uint32_t i = 1; i < 5; ++i) {
    NodeContainer spokeNode = NodeContainer(nodes.Get(i));
    NetDeviceContainer devices = pointToPoint.Install(hubNode, spokeNode);
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  std::cout << "Star topology simulation completed." << std::endl;

  return 0;
}