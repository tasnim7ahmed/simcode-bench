#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

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

  NetDeviceContainer devices;
  for (uint32_t i = 0; i < numNodes - 1; ++i) {
    NetDeviceContainer link = pointToPoint.Install(nodes.Get(i), nodes.Get(i + 1));
    devices.Add(link);
  }

  NetDeviceContainer lastLink = pointToPoint.Install(nodes.Get(numNodes - 1), nodes.Get(0));
  devices.Add(lastLink);

  std::cout << "Ring topology created with " << numNodes << " nodes." << std::endl;

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}