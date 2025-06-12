#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  uint32_t numNodes = 5;

  // Allow command-line override of number of nodes
  CommandLine cmd;
  cmd.AddValue("numNodes", "Number of nodes in the CSMA bus", numNodes);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Configure CSMA channel and devices
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  // Install CSMA devices on all nodes
  NetDeviceContainer devices = csma.Install(nodes);

  // Output confirmation of established connections
  std::cout << "CSMA Bus Topology Setup:" << std::endl;
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<NetDevice> dev = devices.Get(i);
      Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice>(dev);
      if (csmaDev)
        {
          std::cout << " - Node " << i << " connected to CSMA bus, MAC: " 
                    << csmaDev->GetAddress() << std::endl;
        }
      else
        {
          std::cout << " - Node " << i << " not connected properly!" << std::endl;
        }
    }

  Simulator::Stop(Seconds(0.1));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}