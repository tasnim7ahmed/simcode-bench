#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  std::cout << "Nodes created and connected in a bus topology using CSMA." << std::endl;

  Simulator::Run(Seconds(1.0));
  Simulator::Destroy();

  return 0;
}