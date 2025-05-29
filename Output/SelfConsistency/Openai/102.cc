/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Multicast routing example with five nodes (A, B, C, D, E).
 * Node A sends UDP traffic to a multicast group. Nodes B, C, D, E are receivers.
 * Certain links are blacklisted by not adding corresponding routes.
 * Packet sinks and pcap tracing are enabled; statistics are output at simulation end.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MulticastStaticRoutingExample");

// Global counters for statistics
uint32_t packetsReceived = 0;
uint32_t bytesReceived = 0;

void RxCallback (Ptr<const Packet> packet, const Address &from)
{
  packetsReceived++;
  bytesReceived += packet->GetSize ();
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Logging
  LogComponentEnable ("MulticastStaticRoutingExample", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t packetSize = 1024;
  std::string dataRate = "256Kbps";
  double simDuration = 10.0;

  // Nodes: [0] = A, [1] = B, [2] = C, [3] = D, [4] = E
  NodeContainer nodes;
  nodes.Create (5);

  // We connect nodes in a "star" topology: A is the center, each other node gets its own CSMA link with A.
  // This allows us to control routes (blacklist specific links) easily.

  // Create link pairs between A-B, A-C, A-D, A-E
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devAB = csma.Install (NodeContainer (nodes.Get(0), nodes.Get(1)));
  NetDeviceContainer devAC = csma.Install (NodeContainer (nodes.Get(0), nodes.Get(2)));
  NetDeviceContainer devAD = csma.Install (NodeContainer (nodes.Get(0), nodes.Get(3)));
  NetDeviceContainer devAE = csma.Install (NodeContainer (nodes.Get(0), nodes.Get(4)));

  // Install Internet Stack (with multicast routing enabled)
  InternetStackHelper internet;
  Ipv4StaticRoutingHelper multicast;
  internet.SetRoutingHelper (multicast);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceAB = ipv4.Assign (devAB);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceAC = ipv4.Assign (devAC);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceAD = ipv4.Assign (devAD);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceAE = ipv4.Assign (devAE);

  // Multicast group address
  Ipv4Address multicastGroup ("225.1.2.4");

  // Configure multicast static routing
  // Each receiver node's static routing: Join the group on its interface
  // Node A doesn't need to join; it's just source

  // On nodes B, C, D, E join the group on their respective device
  Ptr<NetDevice> devB = devAB.Get (1);
  Ptr<NetDevice> devC = devAC.Get (1);
  Ptr<NetDevice> devD = devAD.Get (1);
  Ptr<NetDevice> devE = devAE.Get (1);

  Ptr<Node> nodeB = nodes.Get (1);
  Ptr<Node> nodeC = nodes.Get (2);
  Ptr<Node> nodeD = nodes.Get (3);
  Ptr<Node> nodeE = nodes.Get (4);

  Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4D = nodeD->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4E = nodeE->GetObject<Ipv4> ();

  uint32_t ifB = ipv4B->GetInterfaceForDevice (devB);
  uint32_t ifC = ipv4C->GetInterfaceForDevice (devC);
  uint32_t ifD = ipv4D->GetInterfaceForDevice (devD);
  uint32_t ifE = ipv4E->GetInterfaceForDevice (devE);

  // Receivers join the multicast group
  ipv4B->SetMulticastReceiveGroup (ifB, multicastGroup);
  ipv4C->SetMulticastReceiveGroup (ifC, multicastGroup);
  ipv4D->SetMulticastReceiveGroup (ifD, multicastGroup);
  ipv4E->SetMulticastReceiveGroup (ifE, multicastGroup);

  // Set static multicast routes
  // Only allow A to send multicast packets to B, C, D, E by listing the correct outgoing interface(s)
  Ptr<Node> nodeA = nodes.Get (0);
  Ptr<Ipv4StaticRouting> multicastStaticRoutingA = multicast.GetStaticRouting (nodeA->GetObject<Ipv4> ());

  // Each subnet interface on A corresponds to one receiver
  // Blacklisting: If you want to blacklist a link (e.g., prevent A->D), just do not add its outgoing interface here

  // For demonstration, let's pretend we want to blacklist the link between A and D
  // (comment out to enable/disable link)
  bool blacklistAD = true;

  multicastStaticRoutingA->AddMulticastRoute (multicastStaticRoutingA->GetIpv4 ()->GetAddress (1, 0).GetLocal (),  // origin (A's iface)
                                             multicastGroup,
                                             {
                                               // device ids for output; do not add devAD.Get(0) to blacklist A-D
                                               devAB.Get (0)->GetIfIndex (),
                                               devAC.Get (0)->GetIfIndex (),
                                               devAE.Get (0)->GetIfIndex ()
                                             });

  // Packet sinks on B, C, D, E, listening on the multicast group UDP port
  uint16_t multicastPort = 5000;

  // Set up packet sinks and connect Rx callbacks
  ApplicationContainer sinks;
  Address mcAddress (InetSocketAddress (multicastGroup, multicastPort));
  for (uint32_t i = 1; i <= 4; ++i)
    {
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", mcAddress);
      sinks.Add (sinkHelper.Install (nodes.Get(i)));
    }
  sinks.Start (Seconds (0.0));
  sinks.Stop (Seconds (simDuration));

  // Connect Rx callbacks for statistics
  for (uint32_t i = 0; i < sinks.GetN(); i++)
    {
      Ptr<Application> sinkApp = sinks.Get(i);
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp);
      sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));
    }

  // Configure OnOff application on node A for UDP traffic to multicast group
  OnOffHelper onoff ("ns3::UdpSocketFactory", mcAddress);
  onoff.SetAttribute ("DataRate", StringValue (dataRate));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simDuration-1)));

  ApplicationContainer apps = onoff.Install (nodes.Get(0));

  // Enable pcap tracing on all csma devices
  csma.EnablePcapAll ("multicast-static-routing", false);

  // Run simulation
  Simulator::Stop (Seconds (simDuration));
  Simulator::Run ();

  // Output statistics
  std::cout << "\n--- Packet Reception Stats (excluding blacklisted links) ---\n"
            << "Total Packets Received: " << packetsReceived << std::endl
            << "Total Bytes Received:   " << bytesReceived << std::endl;

  Simulator::Destroy ();
  return 0;
}