#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  uint32_t numNodes = 4; // Change for more nodes if needed

  NodeContainer nodes;
  nodes.Create (numNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  std::vector<NetDeviceContainer> netDevices; // Store for possible future use

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      for (uint32_t j = i + 1; j < numNodes; ++j)
        {
          NodeContainer pair;
          pair.Add (nodes.Get (i));
          pair.Add (nodes.Get (j));
          NetDeviceContainer devs = p2p.Install (pair);
          netDevices.push_back (devs);
          std::cout << "Established point-to-point link between Node " << i 
                    << " and Node " << j << std::endl;
        }
    }

  Simulator::Stop (Seconds (0.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}