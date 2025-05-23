#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/ipv4.h"
#include <iostream>

int main (int argc, char *argv[])
{
  // Create nodes
  NodeContainer nodes;
  nodes.Create (3); // Create 3 nodes

  // Install Internet Stack on each node
  InternetStackHelper stack;
  stack.Install (nodes);

  // Create a CSMA channel to connect the nodes.
  // This is necessary to create NetDevices, which can then be assigned IP addresses.
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer devices = csma.Install (nodes);

  // Assign unique IP addresses from a base address and mask
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0"); // Base address and subnet mask

  // Assign IP addresses to the NetDevices
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Print the assigned IP addresses
  std::cout << "Assigned IP Addresses:" << std::endl;
  for (uint32_t i = 0; i < interfaces.GetN (); ++i)
  {
    Ptr<NetDevice> device = interfaces.GetNetDevice (i);
    Ptr<Node> node = device->GetNode ();
    Ipv4Address ipAddr = interfaces.GetAddress (i);
    Ipv4Mask ipMask = interfaces.GetMask (i);

    std::cout << "  Node " << node->GetId ()
              << " (NetDevice IfIndex " << device->GetIfIndex () << ")"
              << ": " << ipAddr
              << "/" << ipMask
              << std::endl;
  }

  // The simulator is run only to finalize internal ns-3 configurations.
  // No actual network events or packet transfers are simulated here.
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}