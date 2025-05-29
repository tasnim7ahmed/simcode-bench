#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(3);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(nodes);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    Ipv4Address ipAddr = interfaces.GetAddress(i);
    std::cout << "Node " << node->GetId() << " has IP address: " << ipAddr << std::endl;
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}