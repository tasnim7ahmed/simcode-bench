#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer rootNode;
  rootNode.Create(1);

  NodeContainer intermediateNodes;
  intermediateNodes.Create(2);

  NodeContainer leafNodes;
  leafNodes.Create(4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer rootToInt1Devices = pointToPoint.Install(rootNode.Get(0), intermediateNodes.Get(0));
  NetDeviceContainer rootToInt2Devices = pointToPoint.Install(rootNode.Get(0), intermediateNodes.Get(1));

  NetDeviceContainer int1ToLeaf1Devices = pointToPoint.Install(intermediateNodes.Get(0), leafNodes.Get(0));
  NetDeviceContainer int1ToLeaf2Devices = pointToPoint.Install(intermediateNodes.Get(0), leafNodes.Get(1));

  NetDeviceContainer int2ToLeaf3Devices = pointToPoint.Install(intermediateNodes.Get(1), leafNodes.Get(2));
  NetDeviceContainer int2ToLeaf4Devices = pointToPoint.Install(intermediateNodes.Get(1), leafNodes.Get(3));

  std::cout << "Tree topology created successfully." << std::endl;

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}