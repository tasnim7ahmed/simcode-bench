#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/csma-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FiveNodeMulticast");

int main (int argc, char *argv[])
{
  // Simulation parameters
  double simTime = 5.0;
  uint32_t packetSize = 512;
  std::string dataRate = "500kbps";

  // Create nodes: labels A=0, B=1, C=2, D=3, E=4
  NodeContainer nodes;
  nodes.Create (5);

  // Create topology as a star with A as center, and allow for explicit blacklist control.
  // Let's connect A-B, A-C, A-D, D-E (blacklist: A-E, B-C, B-D, B-E, C-D, C-E)
  // Links:
  //   net1: A<->B
  //   net2: A<->C
  //   net3: A<->D
  //   net4: D<->E

  // Containers for links
  NodeContainer net1 (nodes.Get(0), nodes.Get(1));
  NodeContainer net2 (nodes.Get(0), nodes.Get(2));
  NodeContainer net3 (nodes.Get(0), nodes.Get(3));
  NodeContainer net4 (nodes.Get(3), nodes.Get(4));

  // CSMA Install
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer ndc1 = csma.Install (net1);
  NetDeviceContainer ndc2 = csma.Install (net2);
  NetDeviceContainer ndc3 = csma.Install (net3);
  NetDeviceContainer ndc4 = csma.Install (net4);

  // Install Internet Stack
  InternetStackHelper internet;
  Ipv4StaticRoutingHelper multicast;
  internet.SetRoutingHelper (multicast);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ip;
  ip.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface1 = ip.Assign (ndc1);

  ip.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface2 = ip.Assign (ndc2);

  ip.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface3 = ip.Assign (ndc3);

  ip.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iface4 = ip.Assign (ndc4);

  // Define Multicast group
  Ipv4Address multicastGroup ("225.1.2.4");
  uint16_t multicastPort = 4000;

  // Multicast source is node A (0), sinks are on nodes B (1), C (2), D (3), E (4)
  // Set up multicast routes

  Ptr<Node> srcNode = nodes.Get(0);

  // Sinks join multicast group on their corresponding interface
  // Get interface numbers
  Ptr<NetDevice> ndB = ndc1.Get (1); // Node B's device
  Ptr<NetDevice> ndC = ndc2.Get (1); // Node C's device
  Ptr<NetDevice> ndD1 = ndc3.Get (1); // Node D's device (from A)
  Ptr<NetDevice> ndD2 = ndc4.Get (0); // Node D's device (to E)
  Ptr<NetDevice> ndE = ndc4.Get (1);  // Node E's device

  Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4B = nodes.Get(1)->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4D = nodes.Get(3)->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4E = nodes.Get(4)->GetObject<Ipv4>();

  // Assign interface numbers
  int32_t ifA_B = ipv4A->GetInterfaceForDevice (ndc1.Get (0));
  int32_t ifA_C = ipv4A->GetInterfaceForDevice (ndc2.Get (0));
  int32_t ifA_D = ipv4A->GetInterfaceForDevice (ndc3.Get (0));
  int32_t ifB = ipv4B->GetInterfaceForDevice (ndc1.Get (1));
  int32_t ifC = ipv4C->GetInterfaceForDevice (ndc2.Get (1));
  int32_t ifD_A = ipv4D->GetInterfaceForDevice (ndc3.Get (1));
  int32_t ifD_E = ipv4D->GetInterfaceForDevice (ndc4.Get (0));
  int32_t ifE = ipv4E->GetInterfaceForDevice (ndc4.Get (1));

  // Configure Multicast Group Subscription
  ipv4B->AddMulticastRoute (multicastGroup, ifB);
  ipv4C->AddMulticastRoute (multicastGroup, ifC);
  ipv4D->AddMulticastRoute (multicastGroup, ifD_A); // D listens on A-D link
  ipv4E->AddMulticastRoute (multicastGroup, ifE);

  // Configure Multicast Forwarding

  // On node A: send out via interfaces to B, C, D
  Ipv4StaticRoutingHelper multicastRouting;
  Ptr<Ipv4StaticRouting> routingA = multicastRouting.GetStaticRouting (ipv4A);
  routingA->AddMulticastRoute (multicastGroup, Ipv4Address::GetAny (), {uint32_t(ifA_B), uint32_t(ifA_C), uint32_t(ifA_D)});

  // On node D: incoming on A-D (ifD_A), forward to E (ifD_E) and local
  Ptr<Ipv4StaticRouting> routingD = multicastRouting.GetStaticRouting (ipv4D);
  routingD->AddMulticastRoute (multicastGroup, Ipv4Address::GetAny (), {uint32_t(ifD_E), uint32_t(ifD_A)});

  // (No need to configure forwarding on B, C, or E, as they're only sinks)

  // Enable global multicast routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Packet Sink on B, C, D, E
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), multicastPort));
  ApplicationContainer sinks;
  sinks.Add (sinkHelper.Install (nodes.Get(1))); // B
  sinks.Add (sinkHelper.Install (nodes.Get(2))); // C
  sinks.Add (sinkHelper.Install (nodes.Get(3))); // D
  sinks.Add (sinkHelper.Install (nodes.Get(4))); // E

  sinks.Start (Seconds (0.0));
  sinks.Stop (Seconds (simTime));

  // Set up OnOff traffic generator on A, sending to multicast group
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (multicastGroup, multicastPort));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
  ApplicationContainer onoffs = onoff.Install (nodes.Get(0));

  // Enable PCAP tracing
  csma.EnablePcapAll ("five-node-multicast");

  // Run simulation
  Simulator::Stop (Seconds (simTime + 0.1));
  Simulator::Run ();

  // Collect and output statistics
  uint64_t totalRx = 0;
  uint64_t totalPackets = 0;
  for (uint32_t i = 0; i < sinks.GetN (); ++i)
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinks.Get (i));
      totalRx += sink->GetTotalRx ();
      totalPackets += sink->GetTotalRx () / packetSize;
      std::cout << "Node " << char('B'+i) << " received " << sink->GetTotalRx () << " bytes ("
                << (sink->GetTotalRx () / packetSize) << " packets)" << std::endl;
    }
  std::cout << "Total bytes received by all sinks: " << totalRx << std::endl;
  std::cout << "Total packets received by all sinks: " << totalPackets << std::endl;

  Simulator::Destroy ();
  return 0;
}