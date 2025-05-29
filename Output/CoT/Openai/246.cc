#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  uint32_t numNodes = 4;
  CommandLine cmd;
  cmd.AddValue ("numNodes", "Number of nodes in the topology", numNodes);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  std::vector<NetDeviceContainer> deviceContainers;
  std::vector<Ipv4InterfaceContainer> interfaces;

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      NetDeviceContainer devices = p2p.Install (nodes.Get (i), nodes.Get (i + 1));
      deviceContainers.push_back (devices);

      std::ostringstream subnet;
      subnet << "10.1." << i + 1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaces.push_back (address.Assign (devices));
    }

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      std::cout << "Node " << nodes.Get (i)->GetId () << " created." << std::endl;
    }

  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      std::cout << "Point-to-Point link established between Node "
                << nodes.Get (i)->GetId () << " and Node "
                << nodes.Get (i + 1)->GetId () << std::endl;
    }

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}