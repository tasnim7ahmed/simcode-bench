#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  uint32_t numPeripherals = 5;

  CommandLine cmd;
  cmd.AddValue ("numPeripherals", "Number of peripheral nodes", numPeripherals);
  cmd.Parse (argc, argv);

  // Create nodes: 1 hub + N peripherals
  NodeContainer hub;
  hub.Create (1);
  NodeContainer peripherals;
  peripherals.Create (numPeripherals);

  // Store network devices and point-to-point channels
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  std::vector<NetDeviceContainer> deviceContainers;

  for (uint32_t i = 0; i < numPeripherals; ++i)
    {
      NodeContainer pair (hub.Get (0), peripherals.Get (i));
      NetDeviceContainer devices = p2p.Install (pair);
      deviceContainers.push_back (devices);
    }

  // Print confirmation of established connections
  for (uint32_t i = 0; i < numPeripherals; ++i)
    {
      Ptr<Node> hubNode = hub.Get (0);
      Ptr<Node> peripheralNode = peripherals.Get (i);
      std::cout << "Established Point-to-Point link between Hub (Node "
                << hubNode->GetId ()
                << ") and Peripheral (Node "
                << peripheralNode->GetId () << ")" << std::endl;
    }

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}