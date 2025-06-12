#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // Set time resolution
  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Configure CSMA channel
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  // Install CSMA on nodes
  NetDeviceContainer devices = csma.Install (nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Set up the UDP server on node 2 (index 2: Node 3)
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (2));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Set up the UDP client on node 0 (index 0: Node 1)
  UdpClientHelper client (interfaces.GetAddress (2), port);
  client.SetAttribute ("MaxPackets", UintegerValue (320));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.025))); // 40 packets per second
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}