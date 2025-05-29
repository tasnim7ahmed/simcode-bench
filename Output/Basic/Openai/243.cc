#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  Ptr<Node> node0 = CreateObject<Node>();
  Ptr<Node> node1 = CreateObject<Node>();
  Ptr<Node> node2 = CreateObject<Node>();

  std::cout << "Node 0 ID: " << node0->GetId() << std::endl;
  std::cout << "Node 1 ID: " << node1->GetId() << std::endl;
  std::cout << "Node 2 ID: " << node2->GetId() << std::endl;

  std::cout << "Total nodes created: " << NodeList::GetNNodes() << std::endl;

  return 0;
}