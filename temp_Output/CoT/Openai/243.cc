#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  NS_LOG_UNCOND("Number of nodes created: " << nodes.GetN());

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    NS_LOG_UNCOND("Node ID " << i << " has unique ns-3 ID: " << node->GetId());
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}