#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (5);

  Names::Add ("NodeZero", nodes.Get (0));
  Names::Add ("NodeOne", nodes.Get (1));
  Names::Add ("NodeTwo", nodes.Get (2));
  Names::Add ("NodeThree", nodes.Get (3));
  Names::Add ("NodeFour", nodes.Get (4));

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      std::cout << "Node " << i << " Name: " << Names::FindName (nodes.Get (i))
                << ", Node ID: " << nodes.Get (i)->GetId () << std::endl;
    }

  return 0;
}