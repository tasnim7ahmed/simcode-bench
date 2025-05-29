#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  uint32_t nNodes = 5;

  CommandLine cmd;
  cmd.AddValue("nNodes", "Number of nodes in the CSMA bus", nNodes);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices;
  devices = csma.Install(nodes);

  // Print confirmation of connections
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<NetDevice> device = devices.Get(i);
      Ptr<Node> node = device->GetNode();
      std::cout << "Node " << node->GetId()
                << " connected to CSMA channel with device " << device->GetIfIndex()
                << std::endl;
    }

  Simulator::Stop(Seconds(1.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}