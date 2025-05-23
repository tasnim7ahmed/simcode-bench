#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
  // 1. Create four nodes for the CSMA network
  NodeContainer nodes;
  nodes.Create (4);

  // 2. Configure CSMA link attributes
  CsmaHelper csma;
  // Set data rate to 100 Mbps
  csma.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  // Set delay to 1 millisecond
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

  // 3. Install CSMA devices on nodes and connect them via the shared medium
  NetDeviceContainer devices = csma.Install (nodes);

  // 4. Install the Internet stack (IP, TCP, UDP) on all nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // 5. Assign IP addresses to the network devices
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 6. Setup UDP Echo Server on the third node (index 2)
  // Server will listen on port 9
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  // Server starts at 1.0 second and stops at 10.0 seconds (simulation end)
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // 7. Setup UDP Echo Client on the first node (index 0)
  // Client will send packets to the IP address of the third node (server) on port 9
  // interfaces.GetAddress(2) retrieves the IP address assigned to the device of the third node.
  UdpEchoClientHelper echoClient (interfaces.GetAddress (2), 9);
  // Send 5 packets
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  // Each packet size is 512 bytes
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  // Interval between packets is 1 second
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  // Client starts sending packets at 1.0 second
  clientApps.Start (Seconds (1.0));
  // Client stops at 10.0 seconds (simulation end)
  clientApps.Stop (Seconds (10.0));

  // 8. Set simulation duration to 10 seconds
  Simulator::Stop (Seconds (10.0));

  // 9. Run the simulation
  Simulator::Run ();
  // 10. Clean up simulation resources
  Simulator::Destroy ();

  return 0;
}