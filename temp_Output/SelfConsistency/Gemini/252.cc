#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopology");

int main() {
  LogComponent::SetAttribute("TreeTopology",
                             TypeIdAttributeValue(TypeId::LookupByName("ns3::LogSink::Cout")));

  // Enable logging
  LogComponent::Enable("TreeTopology", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer rootNode;
  rootNode.Create(1);

  NodeContainer intermediateNodes;
  intermediateNodes.Create(2);

  NodeContainer leafNodes;
  leafNodes.Create(4);

  // Create point-to-point helper
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Connect root to intermediate nodes
  NetDeviceContainer rootToInt1Devices, rootToInt2Devices;
  rootToInt1Devices = pointToPoint.Install(rootNode.Get(0), intermediateNodes.Get(0));
  rootToInt2Devices = pointToPoint.Install(rootNode.Get(0), intermediateNodes.Get(1));

  // Connect intermediate nodes to leaf nodes
  NetDeviceContainer int1ToLeaf1Devices, int1ToLeaf2Devices;
  NetDeviceContainer int2ToLeaf3Devices, int2ToLeaf4Devices;

  int1ToLeaf1Devices = pointToPoint.Install(intermediateNodes.Get(0), leafNodes.Get(0));
  int1ToLeaf2Devices = pointToPoint.Install(intermediateNodes.Get(0), leafNodes.Get(1));

  int2ToLeaf3Devices = pointToPoint.Install(intermediateNodes.Get(1), leafNodes.Get(2));
  int2ToLeaf4Devices = pointToPoint.Install(intermediateNodes.Get(1), leafNodes.Get(3));

  // Log connection information
  NS_LOG_INFO("Tree topology created:");
  NS_LOG_INFO("  Root connected to intermediate nodes.");
  NS_LOG_INFO("  Intermediate nodes connected to leaf nodes.");

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}