#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BusTopology");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  uint32_t nNodes = 5;

  NodeContainer nodes;
  nodes.Create (nNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (Seconds (0.02)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  std::cout << "Nodes created: " << nNodes << std::endl;
  std::cout << "CSMA channel established with DataRate=5Mbps and Delay=0.02s" << std::endl;
  std::cout << "IP addresses assigned from 10.1.1.0/24" << std::endl;
  std::cout << "Simulation running for 10 seconds..." << std::endl;

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}