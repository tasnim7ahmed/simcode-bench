#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
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

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  std::cout << "Nodes created and connected via CSMA:" << std::endl;
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    std::cout << "Node " << i << " connected with IP address: " << interfaces.GetAddress(i) << std::endl;
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}