#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Number of peripheral nodes
  uint32_t numNodes = 5;

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes + 1); // +1 for the hub node

  // Hub node is the first node (index 0)
  Ptr<Node> hubNode = nodes.Get(0);

  // Configure point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Connect peripheral nodes to the hub node
  for (uint32_t i = 1; i <= numNodes; ++i) {
    NetDeviceContainer devices = pointToPoint.Install(NodeContainer(hubNode, nodes.Get(i)));
    std::cout << "Connected node " << i << " to the hub." << std::endl;
  }

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}