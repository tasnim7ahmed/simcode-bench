#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ospfv2-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/flow-monitor-module.h"

// Define a logging component for this example
NS_LOG_COMPONENT_DEFINE ("UdpRerouteOspf");

int main (int argc, char *argv[])
{
  // Configure logging
  LogComponentEnable ("UdpRerouteOspf", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("Ospfv2Protocol", LOG_LEVEL_INFO);
  // LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG); // Enable for detailed IP layer logs
  // LogComponentEnable ("Ipv4RoutingProtocol", LOG_LEVEL_DEBUG); // Enable for detailed routing protocol logs

  // Command-line argument to configure OSPF metric for n1-n3 link
  bool useHighMetric = true;
  CommandLine cmd (__FILE__);
  cmd.AddValue ("useHighMetric", "Set OSPF metric for n1-n3 link to a high value to force rerouting (default: true)", useHighMetric);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Node> n2 = nodes.Get (2);
  Ptr<Node> n3 = nodes.Get (3);

  PointToPointHelper p2p;
  NetDeviceContainer devices_n0n2, devices_n1n2, devices_n1n3, devices_n2n3;

  // Configure and install n0-n2 link: 5 Mbps, 2 ms delay
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  devices_n0n2 = p2p.Install (n0, n2);

  // Configure and install n1-n2 link: 5 Mbps, 2 ms delay
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  devices_n1n2 = p2p.Install (n1, n2);

  // Configure and install n1-n3 link: 1.5 Mbps, 100 ms delay (candidate for high cost)
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("100ms"));
  devices_n1n3 = p2p.Install (n1, n3);

  // Configure and install n2-n3 link: 1.5 Mbps, 10 ms delay (alternate path)
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1.5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  devices_n2n3 = p2p.Install (n2, n3);

  // Install Internet stack with OSPF routing
  Ospfv2Helper ospf;
  Ipv4ListRoutingHelper listRouting;
  listRouting.Add (ospf, 0); // OSPF has priority 0
  
  InternetStackHelper stack;
  stack.SetRoutingHelper (listRouting);
  stack.Install (nodes);

  // Assign IP addresses to interfaces
  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces_n0n2, interfaces_n1n2, interfaces_n1n3, interfaces_n2n3;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces_n0n2 = address.Assign (devices_n0n2);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  interfaces_n1n2 = address.Assign (devices_n1n2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  interfaces_n1n3 = address.Assign (devices_n1n3);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  interfaces_n2n3 = address.Assign (devices_n2n3);

  // Set configurable OSPF metric for n1-n3 link to allow for rerouting behavior.
  // The default OSPF cost for a 1.5 Mbps link (assuming 100 Mbps reference bandwidth) is 100/1.5 = 66.
  // The path n3 -> n2 -> n1 has costs:
  //   - n3-n2 link (1.5 Mbps) has default cost 66.
  //   - n2-n1 link (5 Mbps) has default cost 20.
  //   - Total cost for n3-n2-n1 path = 66 + 20 = 86.
  // If `useHighMetric` is true, we set n1-n3 link cost to 1000.
  // This makes the direct n1-n3 path (cost 1000) more expensive than n3-n2-n1 (cost 86),
  // forcing traffic to reroute via n2.
  if (useHighMetric)
  {
    uint32_t highMetric = 1000;
    // Set metric for n1's interface connected to n3
    ospf.EnableInterface (devices_n1n3.Get (0), highMetric);
    // Set metric for n3's interface connected to n1
    ospf.EnableInterface (devices_n1n3.Get (1), highMetric);
    NS_LOG_INFO ("Configured OSPF metric for n1-n3 link to " << highMetric << " to force rerouting.");
  }
  else
  {
    // If useHighMetric is false, OSPF will calculate the default cost based on bandwidth
    // (e.g., 66 for 1.5 Mbps), making the direct n1-n3 path (cost 66) cheaper than n3-n2-n1 (cost 86).
    NS_LOG_INFO ("Using default OSPF metric for n1-n3 link (calculated based on bandwidth).");
  }

  // Set up UDP traffic from n3 to n1
  uint16_t port = 9; // Discard port

  // Install UDP Server on n1
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (n1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP Client on n3, targeting n1's IP address on the n1-n2 subnet.
  // This ensures the client targets an IP on a network segment that allows the rerouted path via n2.
  UdpClientHelper client (interfaces_n1n2.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100)); // Send 100 packets
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1))); // Send one packet every 0.1 seconds (10 packets/sec)
  client.SetAttribute ("PacketSize", UintegerValue (1024)); // 1 KB packets
  ApplicationContainer clientApps = client.Install (n3);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable tracing
  // Creates a .tr file for ASCII tracing of queues and packet receptions
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("udp-reroute-ospf.tr"));
  
  // Set up Flow Monitor for logging simulation results
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  // Set simulation stop time
  Simulator::Stop (Seconds (11.0));

  // Run simulation
  Simulator::Run ();

  // Generate Flow Monitor XML output
  monitor->CheckForError ();
  monitor->SerializeToXmlFile ("udp-reroute-ospf-flowmon.xml", true, true);

  // Clean up
  Simulator::Destroy ();

  return 0;
}