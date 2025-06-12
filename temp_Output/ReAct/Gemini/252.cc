#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  uint32_t nRootChildren = 2;
  uint32_t nIntermediateChildren = 2;

  NodeContainer rootNode;
  rootNode.Create(1);

  NodeContainer intermediateNodes;
  intermediateNodes.Create(nRootChildren);

  NodeContainer leafNodes;
  leafNodes.Create(nRootChildren * nIntermediateChildren);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Root to Intermediate
  NetDeviceContainer rootToIntermediateDevices;
  for (uint32_t i = 0; i < nRootChildren; ++i) {
    NodeContainer pair;
    pair.Add(rootNode.Get(0));
    pair.Add(intermediateNodes.Get(i));
    NetDeviceContainer devices = pointToPoint.Install(pair);
    rootToIntermediateDevices.Add(devices.Get(0));
    rootToIntermediateDevices.Add(devices.Get(1));
  }

  // Intermediate to Leaf
  NetDeviceContainer intermediateToLeafDevices;
  for (uint32_t i = 0; i < nRootChildren; ++i) {
    for (uint32_t j = 0; j < nIntermediateChildren; ++j) {
      NodeContainer pair;
      pair.Add(intermediateNodes.Get(i));
      pair.Add(leafNodes.Get(i * nIntermediateChildren + j));
      NetDeviceContainer devices = pointToPoint.Install(pair);
      intermediateToLeafDevices.Add(devices.Get(0));
      intermediateToLeafDevices.Add(devices.Get(1));
    }
  }

  std::cout << "Network topology created successfully." << std::endl;

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}